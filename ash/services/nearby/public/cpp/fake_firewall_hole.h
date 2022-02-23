// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_FIREWALL_HOLE_H_
#define ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_FIREWALL_HOLE_H_

#include "ash/services/nearby/public/mojom/firewall_hole.mojom.h"

namespace ash {
namespace nearby {

// A trivial implementation of sharing::mojom::FirewallHole used for testing.
class FakeFirewallHole : public sharing::mojom::FirewallHole {
 public:
  FakeFirewallHole() = default;
  ~FakeFirewallHole() override = default;
};

}  // namespace nearby
}  // namespace ash

#endif  // ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_FIREWALL_HOLE_H_
