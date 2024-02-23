// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "ash/quick_pair/feature_status_tracker/mock_battery_saver_active_provider.h"
#include "ash/quick_pair/feature_status_tracker/mock_fast_pair_pref_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/mock_hardware_offloading_supported_provider.h"
#include "ash/quick_pair/feature_status_tracker/mock_power_connected_provider.h"
#include "ash/quick_pair/feature_status_tracker/scanning_enabled_provider.h"

namespace {
constexpr int kNumScanningEnabledProviderCtorArgs = 4;

PrefService* GetLocalState() {
  ash::Shell* shell = ash::Shell::Get();
  CHECK(shell);
  PrefService* pref_service = shell->local_state();
  CHECK(pref_service);
  return pref_service;
}

}  // namespace

namespace ash::quick_pair {

class ScanningEnabledProviderTestBase : public AshTestBase {
 public:
  void TearDown() override {
    battery_saver_provider_ = nullptr;
    fast_pair_pref_enabled_provider_ = nullptr;
    hardware_offloading_provider_ = nullptr;
    power_connected_provider_ = nullptr;
    scanning_enabled_provider_.reset();

    AshTestBase::TearDown();
  }

  void SetSoftwareScanningStatus(
      ScanningEnabledProvider::SoftwareScanningStatus status) {
    PrefService* local_state = GetLocalState();
    local_state->SetInteger(ash::prefs::kSoftwareScanningEnabled,
                            static_cast<int>(status));
  }

  bool IsScanningEnabled() {
    return scanning_enabled_provider_ &&
           scanning_enabled_provider_->is_enabled();
  }

 protected:
  raw_ptr<MockBatterySaverActiveProvider> battery_saver_provider_ = nullptr;
  raw_ptr<MockFastPairPrefEnabledProvider> fast_pair_pref_enabled_provider_ =
      nullptr;
  raw_ptr<MockHardwareOffloadingSupportedProvider>
      hardware_offloading_provider_ = nullptr;
  raw_ptr<MockPowerConnectedProvider> power_connected_provider_ = nullptr;
  std::unique_ptr<ScanningEnabledProvider> scanning_enabled_provider_;
};

class ScanningEnabledProviderTest : public ScanningEnabledProviderTestBase {
 public:
  void InitializeProviders(bool is_battery_saver_enabled,
                           bool is_fast_pair_pref_enabled,
                           bool is_hardware_offloading_enabled,
                           bool is_power_connected_enabled) {
    battery_saver_provider_ = new MockBatterySaverActiveProvider();
    fast_pair_pref_enabled_provider_ = new MockFastPairPrefEnabledProvider();
    hardware_offloading_provider_ =
        new MockHardwareOffloadingSupportedProvider();
    power_connected_provider_ = new MockPowerConnectedProvider();

    SetProvidersEnabled(is_battery_saver_enabled, is_fast_pair_pref_enabled,
                        is_hardware_offloading_enabled,
                        is_power_connected_enabled);

    scanning_enabled_provider_ = std::make_unique<ScanningEnabledProvider>(
        std::unique_ptr<BatterySaverActiveProvider>(battery_saver_provider_),
        std::unique_ptr<FastPairPrefEnabledProvider>(
            fast_pair_pref_enabled_provider_),
        std::unique_ptr<HardwareOffloadingSupportedProvider>(
            hardware_offloading_provider_),
        std::unique_ptr<PowerConnectedProvider>(power_connected_provider_));
  }

  void SetProvidersEnabled(bool is_battery_saver_enabled,
                           bool is_fast_pair_pref_enabled,
                           bool is_hardware_offloading_enabled,
                           bool is_power_connected_enabled) {
    CHECK(battery_saver_provider_);
    CHECK(fast_pair_pref_enabled_provider_);
    CHECK(hardware_offloading_provider_);
    CHECK(power_connected_provider_);

    ON_CALL(*battery_saver_provider_, is_enabled)
        .WillByDefault(testing::Return(is_battery_saver_enabled));

    ON_CALL(*fast_pair_pref_enabled_provider_, is_enabled)
        .WillByDefault(testing::Return(is_fast_pair_pref_enabled));

    ON_CALL(*hardware_offloading_provider_, is_enabled)
        .WillByDefault(testing::Return(is_hardware_offloading_enabled));

    ON_CALL(*power_connected_provider_, is_enabled)
        .WillByDefault(testing::Return(is_power_connected_enabled));
  }
};

// Test name anatomy: Scanning(En/Dis)abledOn[factors under test]_[remaining
// factors in scanning enabled calculation]. If scanning is expected to be
// enabled, the remaining factors will be set such that if the factors under
// test are not as expected, scanning should be disabled. Likewise, if expected
// to be disabled, the remaining factors will be set so that if the factors
// under test are unexpected, scanning will be enabled.
TEST_F(
    ScanningEnabledProviderTest,
    ScanningEnabledOnHardwareOffloadingSupportedFastPairPrefEnabled_BatterySaverActive_StatusNever_PowerDisconnected) {
  InitializeProviders(/*is_battery_saver_enabled=*/true,
                      /*is_fast_pair_pref_enabled=*/true,
                      /*is_hardware_offloading_enabled=*/true,
                      /*is_power_connected_enabled=*/false);
  SetSoftwareScanningStatus(
      ScanningEnabledProvider::SoftwareScanningStatus::kNever);
  EXPECT_TRUE(IsScanningEnabled());
}

TEST_F(
    ScanningEnabledProviderTest,
    ScanningDisabledOnHardwareOffloadingSupportedFastPairPrefDisabled_BatterySaverActive_StatusNever_PowerDisconnected) {
  InitializeProviders(/*is_battery_saver_enabled=*/true,
                      /*is_fast_pair_pref_enabled=*/false,
                      /*is_hardware_offloading_enabled=*/true,
                      /*is_power_connected_enabled=*/false);
  SetSoftwareScanningStatus(
      ScanningEnabledProvider::SoftwareScanningStatus::kAlways);
  EXPECT_FALSE(IsScanningEnabled());
}

TEST_F(
    ScanningEnabledProviderTest,
    ScanningDisabledOnBatterySaverActive_HardwareOffloadingUnsupported_FastPairPrefEnabled_StatusAlways_PowerConnected) {
  InitializeProviders(/*is_battery_saver_enabled=*/true,
                      /*is_fast_pair_pref_enabled=*/true,
                      /*is_hardware_offloading_enabled=*/false,
                      /*is_power_connected_enabled=*/true);
  SetSoftwareScanningStatus(
      ScanningEnabledProvider::SoftwareScanningStatus::kAlways);
  EXPECT_FALSE(IsScanningEnabled());
}

TEST_F(
    ScanningEnabledProviderTest,
    ScanningEnabledOnStatusAlways_HardwareOffloadingUnsupported_FastPairPrefDisabled_BatterySaverInactive_PowerDisconnected) {
  InitializeProviders(/*is_battery_saver_enabled=*/false,
                      /*is_fast_pair_pref_enabled=*/false,
                      /*is_hardware_offloading_enabled=*/false,
                      /*is_power_connected_enabled=*/false);
  SetSoftwareScanningStatus(
      ScanningEnabledProvider::SoftwareScanningStatus::kAlways);
  EXPECT_TRUE(IsScanningEnabled());
}

TEST_F(
    ScanningEnabledProviderTest,
    ScanningDisabledOnStatusWhenChargingAndPowerDisconnected_HardwareOffloadingUnsupported_FastPairPrefEnabled_BatterySaverInactive) {
  InitializeProviders(/*is_battery_saver_enabled=*/false,
                      /*is_fast_pair_pref_enabled=*/true,
                      /*is_hardware_offloading_enabled=*/false,
                      /*is_power_connected_enabled=*/false);
  SetSoftwareScanningStatus(
      ScanningEnabledProvider::SoftwareScanningStatus::kOnlyWhenCharging);
  EXPECT_FALSE(IsScanningEnabled());
}

TEST_F(
    ScanningEnabledProviderTest,
    ScanningEnabledOnStatusWhenChargingAndPowerConnected_HardwareOffloadingUnsupported_FastPairPrefDisabled_BatterySaverInactive) {
  InitializeProviders(/*is_battery_saver_enabled=*/false,
                      /*is_fast_pair_pref_enabled=*/false,
                      /*is_hardware_offloading_enabled=*/false,
                      /*is_power_connected_enabled=*/true);
  SetSoftwareScanningStatus(
      ScanningEnabledProvider::SoftwareScanningStatus::kOnlyWhenCharging);
  EXPECT_TRUE(IsScanningEnabled());
}

TEST_F(
    ScanningEnabledProviderTest,
    ScanningDisabledOnStatusNever_HardwareOffloadingUnsupported_FastPairPrefEnabled_BatterySaverInactive_PowerConnected) {
  InitializeProviders(/*is_battery_saver_enabled=*/false,
                      /*is_fast_pair_pref_enabled=*/true,
                      /*is_hardware_offloading_enabled=*/false,
                      /*is_power_connected_enabled=*/true);
  SetSoftwareScanningStatus(
      ScanningEnabledProvider::SoftwareScanningStatus::kNever);
  EXPECT_FALSE(IsScanningEnabled());
}

class ScanningEnabledProviderNoCrashOnNullInputsTest
    : public ScanningEnabledProviderTest,
      public testing::WithParamInterface<size_t> {
 public:
  // Initialize each argument to `scanning_enabled_provider_` with either
  // a `unique_ptr` of expected type or a `nullptr` depending on whether the bit
  // in `args_mask` corresponding to the argument's place in order is 1 or 0,
  // respectively. Return values to each mock's `is_enabled` such that,
  // in the evaluation of the logical expression in
  // `ScanningEnabledProvider::IsScanningEnabled`, each subprovider will be
  // accessed if the providers accessed before it in evaluation order are
  // non-null except for the FastPairPrefEnabledProvider, which will be checked
  // in the case that only it and the HardwareOffloadingSupportedProvider are
  // non-null and all other providers are null.
  void InitializeProviders(size_t args_mask) {
    bool is_battery_saver_nonnull = args_mask & 1;
    bool is_fast_pair_pref_nonnull = args_mask & 2;
    bool is_hardware_offloading_nonnull = args_mask & 4;
    bool is_power_connected_nonnull = args_mask & 8;

    if (is_battery_saver_nonnull) {
      battery_saver_provider_ = new MockBatterySaverActiveProvider();
      ON_CALL(*battery_saver_provider_, is_enabled)
          .WillByDefault(testing::Return(false));
    }

    if (is_fast_pair_pref_nonnull) {
      fast_pair_pref_enabled_provider_ = new MockFastPairPrefEnabledProvider();
      ON_CALL(*fast_pair_pref_enabled_provider_, is_enabled)
          .WillByDefault(testing::Return(true));
    }

    if (is_hardware_offloading_nonnull) {
      hardware_offloading_provider_ =
          new MockHardwareOffloadingSupportedProvider();
      ON_CALL(*hardware_offloading_provider_, is_enabled)
          .WillByDefault(testing::Return(
              is_hardware_offloading_nonnull && is_fast_pair_pref_nonnull &&
              !is_battery_saver_nonnull && !is_power_connected_nonnull));
    }

    if (is_power_connected_nonnull) {
      power_connected_provider_ = new MockPowerConnectedProvider();
      ON_CALL(*power_connected_provider_, is_enabled)
          .WillByDefault(testing::Return(false));
    }

    scanning_enabled_provider_ = std::make_unique<ScanningEnabledProvider>(
        (is_battery_saver_nonnull ? std::unique_ptr<BatterySaverActiveProvider>(
                                        battery_saver_provider_)
                                  : nullptr),
        (is_fast_pair_pref_nonnull
             ? std::unique_ptr<FastPairPrefEnabledProvider>(
                   fast_pair_pref_enabled_provider_)
             : nullptr),
        (is_hardware_offloading_nonnull
             ? std::unique_ptr<HardwareOffloadingSupportedProvider>(
                   hardware_offloading_provider_)
             : nullptr),
        (is_power_connected_nonnull ? std::unique_ptr<PowerConnectedProvider>(
                                          power_connected_provider_)
                                    : nullptr));
  }
};

// This parametrized test initializes `scanning_enabled_provider_` with all
// combinations of `nullptr` and non-null inputs. If it doesn't crash, it's
// considered passed.
TEST_P(ScanningEnabledProviderNoCrashOnNullInputsTest, NoCrashOnNullInputs) {
  SetSoftwareScanningStatus(
      ScanningEnabledProvider::SoftwareScanningStatus::kAlways);
  InitializeProviders(/*args_mask=*/GetParam());
  IsScanningEnabled();
}

INSTANTIATE_TEST_SUITE_P(
    ScanningEnabledProviderNoCrashOnNullInputsTest,
    ScanningEnabledProviderNoCrashOnNullInputsTest,
    testing::Range<size_t>(0, 1 << kNumScanningEnabledProviderCtorArgs));

}  // namespace ash::quick_pair
