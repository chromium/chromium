// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/model/scoped_fake_power_status.h"

#include <memory>

#include "ash/system/model/fake_power_status.h"
#include "ash/system/power/power_status.h"
#include "ash/test/ash_test_base.h"

namespace ash {

using ScopedFakePowerStatusTest = AshTestBase;

TEST_F(ScopedFakePowerStatusTest, Basic) {
  auto* real_instance = PowerStatus::Get();

  // Scoped setter is created. PowerStatus::Get() should point to the fake
  // instance.
  auto scoped_fake_power_status = std::make_unique<ScopedFakePowerStatus>();

  EXPECT_EQ(scoped_fake_power_status->fake_power_status(),
            static_cast<FakePowerStatus*>(PowerStatus::Get()));

  // When the scoped setter is removed. All the public getters should now point
  // to the previous real instance.
  scoped_fake_power_status.reset();
  EXPECT_EQ(real_instance, PowerStatus::Get());
}
}  // namespace ash
