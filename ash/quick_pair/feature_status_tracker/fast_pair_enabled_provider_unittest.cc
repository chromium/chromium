// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/fast_pair_enabled_provider.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/common/fake_bluetooth_adapter.h"
#include "ash/quick_pair/feature_status_tracker/bluetooth_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/fast_pair_pref_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/logged_in_user_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/mock_bluetooth_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/mock_fast_pair_pref_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/mock_google_api_key_availability_provider.h"
#include "ash/quick_pair/feature_status_tracker/mock_logged_in_user_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/mock_scanning_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/mock_screen_state_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/screen_state_enabled_provider.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr int kNumFastPairEnabledProviderArgs = 6;
}  // namespace

namespace ash::quick_pair {

class FastPairEnabledProviderTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
  }

 protected:
  scoped_refptr<FakeBluetoothAdapter> adapter_;
};

TEST_F(FastPairEnabledProviderTest, ProviderCallbackIsInvokedOnBTChanges) {
  base::test::ScopedFeatureList feature_list{features::kFastPair};

  base::MockCallback<base::RepeatingCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(true));

  auto* fast_pair_pref_enabled_provider = new MockFastPairPrefEnabledProvider();
  ON_CALL(*fast_pair_pref_enabled_provider, is_enabled)
      .WillByDefault(testing::Return(true));

  auto* logged_in_user_enabled_provider = new MockLoggedInUserEnabledProvider();
  ON_CALL(*logged_in_user_enabled_provider, is_enabled)
      .WillByDefault(testing::Return(true));

  auto* screen_state_enabled_provider = new MockScreenStateEnabledProvider();
  ON_CALL(*screen_state_enabled_provider, is_enabled)
      .WillByDefault(testing::Return(true));

  auto* google_api_key_availability_provider =
      new MockGoogleApiKeyAvailabilityProvider();
  ON_CALL(*google_api_key_availability_provider, is_enabled)
      .WillByDefault(testing::Return(true));

  auto* scanning_enabled_provider = new MockScanningEnabledProvider();
  ON_CALL(*scanning_enabled_provider, is_enabled)
      .WillByDefault(testing::Return(true));

  auto provider = std::make_unique<FastPairEnabledProvider>(
      std::make_unique<BluetoothEnabledProvider>(),
      base::WrapUnique(fast_pair_pref_enabled_provider),
      base::WrapUnique(logged_in_user_enabled_provider),
      base::WrapUnique(screen_state_enabled_provider),
      base::WrapUnique(google_api_key_availability_provider),
      base::WrapUnique(scanning_enabled_provider));

  provider->SetCallback(callback.Get());

  adapter_->SetBluetoothIsPowered(true);
}

TEST_F(FastPairEnabledProviderTest, IsEnabledWhenExpected) {
  base::test::ScopedFeatureList feature_list;
  const base::flat_map<base::test::FeatureRef, bool> feature_states{
      {features::kFastPair, true}};
  feature_list.InitWithFeatureStates(feature_states);

  auto* bluetooth_enabled_provider = new MockBluetoothEnabledProvider();
  ON_CALL(*bluetooth_enabled_provider, is_enabled)
      .WillByDefault(testing::Return(true));

  auto* fast_pair_pref_enabled_provider = new MockFastPairPrefEnabledProvider();
  ON_CALL(*fast_pair_pref_enabled_provider, is_enabled)
      .WillByDefault(testing::Return(true));

  auto* logged_in_user_enabled_provider = new MockLoggedInUserEnabledProvider();
  ON_CALL(*logged_in_user_enabled_provider, is_enabled)
      .WillByDefault(testing::Return(true));

  auto* screen_state_enabled_provider = new MockScreenStateEnabledProvider();
  ON_CALL(*screen_state_enabled_provider, is_enabled)
      .WillByDefault(testing::Return(true));

  auto* google_api_key_availability_provider =
      new MockGoogleApiKeyAvailabilityProvider();
  ON_CALL(*google_api_key_availability_provider, is_enabled)
      .WillByDefault(testing::Return(true));

  auto* scanning_enabled_provider = new MockScanningEnabledProvider();
  ON_CALL(*scanning_enabled_provider, is_enabled)
      .WillByDefault(testing::Return(true));

  auto provider = std::make_unique<FastPairEnabledProvider>(
      std::unique_ptr<BluetoothEnabledProvider>(bluetooth_enabled_provider),
      std::unique_ptr<FastPairPrefEnabledProvider>(
          fast_pair_pref_enabled_provider),
      std::unique_ptr<LoggedInUserEnabledProvider>(
          logged_in_user_enabled_provider),
      std::unique_ptr<ScreenStateEnabledProvider>(
          screen_state_enabled_provider),
      base::WrapUnique(google_api_key_availability_provider),
      base::WrapUnique(scanning_enabled_provider));

  EXPECT_TRUE(provider->is_enabled());
}

class FastPairEnabledProviderTestNoCrashOnNullInputs
    : public FastPairEnabledProviderTest,
      public testing::WithParamInterface<size_t> {};

TEST_P(FastPairEnabledProviderTestNoCrashOnNullInputs, NoCrashOnNullInputs) {
  base::test::ScopedFeatureList feature_list;
  const base::flat_map<base::test::FeatureRef, bool> feature_states{
      {features::kFastPair, true}};
  feature_list.InitWithFeatureStates(feature_states);

  size_t args_mask = GetParam();
  bool is_bluetooth_provider_nonnull = args_mask & 1;
  bool is_fast_pair_pref_nonnull = args_mask & 2;
  bool is_user_enabled_provider_nonnull = args_mask & 4;
  bool is_screen_enabled_provider_nonnull = args_mask & 8;
  bool is_api_provider_nonnull = args_mask & 16;
  bool is_scanning_provider_nonnull = args_mask & 32;

  std::unique_ptr<BluetoothEnabledProvider> bluetooth_enabled_provider;
  if (is_bluetooth_provider_nonnull) {
    bluetooth_enabled_provider = std::make_unique<BluetoothEnabledProvider>();
  }

  MockFastPairPrefEnabledProvider* fast_pair_pref_enabled_provider = nullptr;
  if (is_fast_pair_pref_nonnull) {
    fast_pair_pref_enabled_provider = new MockFastPairPrefEnabledProvider();
  }

  MockLoggedInUserEnabledProvider* logged_in_user_enabled_provider = nullptr;
  if (is_user_enabled_provider_nonnull) {
    logged_in_user_enabled_provider = new MockLoggedInUserEnabledProvider();
  }

  MockScreenStateEnabledProvider* screen_state_enabled_provider = nullptr;
  if (is_screen_enabled_provider_nonnull) {
    screen_state_enabled_provider = new MockScreenStateEnabledProvider();
  }

  MockGoogleApiKeyAvailabilityProvider* google_api_key_availability_provider =
      nullptr;
  if (is_api_provider_nonnull) {
    google_api_key_availability_provider =
        new MockGoogleApiKeyAvailabilityProvider();
  }

  MockScanningEnabledProvider* scanning_enabled_provider = nullptr;
  if (is_scanning_provider_nonnull) {
    scanning_enabled_provider = new MockScanningEnabledProvider();
  }

  std::unique_ptr<FastPairEnabledProvider> fast_pair_enabled_provider =
      std::make_unique<FastPairEnabledProvider>(
          std::move(bluetooth_enabled_provider),
          base::WrapUnique(fast_pair_pref_enabled_provider),
          base::WrapUnique(logged_in_user_enabled_provider),
          base::WrapUnique(screen_state_enabled_provider),
          base::WrapUnique(google_api_key_availability_provider),
          base::WrapUnique(scanning_enabled_provider));

  fast_pair_enabled_provider->is_enabled();
}

INSTANTIATE_TEST_SUITE_P(
    FastPairEnabledProviderTestNoCrashOnNullInputs,
    FastPairEnabledProviderTestNoCrashOnNullInputs,
    testing::Range<size_t>(0, 1 << kNumFastPairEnabledProviderArgs));

}  // namespace ash::quick_pair
