// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/battery_saver_active_provider.h"

#include "ash/display/refresh_rate_controller.h"
#include "ash/shell.h"
#include "ash/system/power/battery_saver_controller.h"
#include "ash/system/power/power_status.h"
#include "ash/test/ash_test_base.h"

namespace ash {
namespace quick_pair {

class BatterySaverActiveProviderTest : public AshTestBase {
 public:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    // This test may delete/recreate PowerStatus, which can cause dangling
    // pointer in RefreshRateController which observes the power status. Let it
    // forget the power status so that it doesn't reported as a dangling
    // pointer.
    ash::Shell::Get()
        ->refresh_rate_controller()
        ->StopObservingPowerStatusForTest();
    // And BatterySaverController.
    ash::Shell::Get()
        ->battery_saver_controller()
        ->StopObservingPowerStatusForTest();
  }

  // PowerStatus must be initialized before AshTestBase::TearDown() because the
  // latter calls PowerStatus::Shutdown, which crashes if the PowerStatus is not
  // initialized.
  void TearDown() override {
    battery_saver_active_provider_.reset();
    if (!PowerStatus::IsInitialized()) {
      PowerStatus::Initialize();
    }
    AshTestBase::TearDown();
  }

  void SetBatterySaverStateAndNotifyProvider(bool is_enabled) {
    if (!battery_saver_active_provider_) {
      return;
    }

    if (!PowerStatus::IsInitialized()) {
      return;
    }

    PowerStatus::Get()->SetBatterySaverStateForTesting(is_enabled);
    battery_saver_active_provider_->OnPowerStatusChanged();
  }

  void InitializeProvider() {
    battery_saver_active_provider_ =
        std::make_unique<BatterySaverActiveProvider>();
  }

  void InitializePowerStatus(bool should_initialize) {
    if (should_initialize && !PowerStatus::IsInitialized()) {
      PowerStatus::Initialize();
    } else if (!should_initialize && PowerStatus::IsInitialized()) {
      PowerStatus::Shutdown();
    }
    CHECK(should_initialize == PowerStatus::IsInitialized());
  }

  bool IsBatterySaverActive() {
    return battery_saver_active_provider_->is_enabled();
  }

 protected:
  std::unique_ptr<BatterySaverActiveProvider> battery_saver_active_provider_;
};

TEST_F(BatterySaverActiveProviderTest,
       ProviderDisabledIfPowerStatusNotInitialized) {
  InitializePowerStatus(/*should_initialize=*/false);
  InitializeProvider();
  EXPECT_FALSE(IsBatterySaverActive());
}

TEST_F(BatterySaverActiveProviderTest,
       ProviderEnabledOnBatterySaverModeEnabled) {
  InitializePowerStatus(/*should_initialize=*/true);
  InitializeProvider();
  SetBatterySaverStateAndNotifyProvider(/*is_enabled=*/true);
  EXPECT_TRUE(IsBatterySaverActive());
}

TEST_F(BatterySaverActiveProviderTest,
       ProviderDisabledOnBatterySaverModeDisabled) {
  InitializePowerStatus(/*should_initialize=*/true);
  InitializeProvider();
  SetBatterySaverStateAndNotifyProvider(/*is_enabled=*/false);
  EXPECT_FALSE(IsBatterySaverActive());
}

}  // namespace quick_pair
}  // namespace ash
