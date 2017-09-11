// This file is a part of the IncludeOS unikernel - www.includeos.org
//
// Copyright 2017 Oslo and Akershus University College of Applied Sciences
// and Alfred Bratterud
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <common.cxx>
#include <net/conntrack.hpp>

CASE("Testing Conntrack flow")
{
  using namespace net;
  const Protocol proto{Protocol::UDP};
  Socket src{{10,0,0,42}, 80};
  Socket dst{{10,0,0,1}, 1337};
  Quadruple quad{src, dst};
  // Reversed quadruple
  Quadruple rquad = quad; rquad.swap();

  Conntrack ct;
  Conntrack::Entry* entry = nullptr;

  // Entry do not exist
  EXPECT((entry = ct.get(quad, proto)) == nullptr);

  // We now track it
  EXPECT((entry = ct.simple_track_in(quad, proto)) != nullptr);

  // It should now have state NEW
  EXPECT(entry->state == Conntrack::State::UNCONFIRMED);
  EXPECT(entry->proto == proto);
  // The timeout should be set to "timeout_unconfirmed"
  EXPECT(entry->timeout == RTC::now() + ct.timeout_unconfirmed.count());
  // It's quad values should be set to quad and rquad
  EXPECT(entry->first == quad);
  EXPECT(entry->second == rquad);

  // We can get the entry
  EXPECT(ct.get(quad, proto) == entry);

  // Confirm works
  EXPECT(ct.confirm(quad, proto) == entry);
  EXPECT(entry->state == Conntrack::State::NEW);

  // The timeout should now be updated to "timeout_new" when confirmed
  EXPECT(entry->timeout == RTC::now() + ct.timeout_new.count());

  // Confirming it again wont have any effect
  EXPECT(ct.confirm(quad, proto) != nullptr);

  // We can now get the entry after being confirmed
  EXPECT(entry == ct.get(quad, proto));

  // Can also get the same entry by using the reversed entry
  EXPECT(entry == ct.get(rquad, proto));

  // We now track the "reply" by using the reversed quadruple
  EXPECT(entry == ct.simple_track_in(rquad, proto));

  // The entry should now be ESTABLISHED due to seen traffic both ways
  EXPECT(entry->state == Conntrack::State::ESTABLISHED);
  // The timeout should be set to "timeout_established"
  EXPECT(entry->timeout == RTC::now() + ct.timeout_established.count());

  // Setup a custom on_close event
  bool closed = false;
  entry->on_close = [&closed](auto*){ closed = true; };
  // Lets pretend there has been no traffic for a while, and the flush timer fires
  entry->timeout = RTC::now();
  ct.remove_expired();
  // Our entry should no longer be found in the tracker
  EXPECT((entry = ct.get(quad, proto)) == nullptr);
  // The on_close event was called
  EXPECT(closed == true);

}

CASE("Testing Conntrack update entry")
{
  using namespace net;
  const Protocol proto{Protocol::UDP};
  Socket src{{10,0,0,42}, 80};
  Socket dst{{10,0,0,1}, 1337};
  Quadruple quad{src, dst};
  // Reversed quadruple
  Quadruple rquad = quad; rquad.swap();

  Conntrack ct;
  Conntrack::Entry* entry = nullptr;

  entry = ct.simple_track_in(quad, proto);
  ct.confirm(quad, proto);
  ct.simple_track_in(rquad, proto);

  // The entry can be get() both ways
  EXPECT(entry == ct.get(quad, proto));
  EXPECT(entry == ct.get(rquad, proto));

  EXPECT(entry->first == quad);
  EXPECT(entry->second == rquad);

  // Let's update the second quadruple with a new one
  Socket new_src{{10,0,0,42}, 80};
  Socket new_dst{{10,0,0,1}, 1337};
  Quadruple new_quad{new_src, new_dst};
  ct.update_entry(proto, entry->second, new_quad);

  // The entry can still be get on the first quad
  EXPECT(entry == ct.get(quad, proto));
  // Not on the old one
  EXPECT(ct.get(rquad, proto) == nullptr);
  // But the new one
  EXPECT(entry == ct.get(new_quad, proto));
  // The entry had it's value updated
  EXPECT(entry->first == quad);
  EXPECT(entry->second == new_quad);
}
