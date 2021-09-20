// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/fast_pair_enabled_provider.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/feature_status_tracker/bluetooth_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/fake_bluetooth_adapter.h"
#include "ash/quick_pair/feature_status_tracker/logged_in_user_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/mock_bluetooth_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/mock_google_api_key_availability_provider.h"
#include "ash/quick_pair/feature_status_tracker/mock_logged_in_user_enabled_provider.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {

class FastPairEnabledProviderTest : public testing::Test {
 public:
  void SetUp() override {
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<user_manager::FakeUserManager>());
  }

 protected:
  scoped_refptr<FakeBluetoothAdapter> adapter_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

TEST_F(FastPairEnabledProviderTest, ProviderCallbackIsInvokedOnBTChanges) {
  base::test::ScopedFeatureList feature_list{features::kFastPair};

  base::MockCallback<base::RepeatingCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(true));

  auto* logged_in_user_enabled_provider = new MockLoggedInUserEnabledProvider();
  ON_CALL(*logged_in_user_enabled_provider, is_enabled)
      .WillByDefault(testing::Return(true));

  auto* google_api_key_availability_provider =
      new MockGoogleApiKeyAvailabilityProvider();
  ON_CALL(*google_api_key_availability_provider, is_enabled)
      .WillByDefault(testing::Return(true));

  auto provider = std::make_unique<FastPairEnabledProvider>(
      std::make_unique<BluetoothEnabledProvider>(),
      base::WrapUnique(logged_in_user_enabled_provider),
      base::WrapUnique(google_api_key_availability_provider));

  provider->SetCallback(callback.Get());

  adapter_->NotifyPoweredChanged(true);
}

// Represents: <is_flag_enabled, is_bt_enabled, is_user_logged_in,
//              is_google_api_keys_available>
using TestParam = std::tuple<bool, bool, bool, bool>;

class FastPairEnabledProviderTestWithParams
    : public FastPairEnabledProviderTest,
      public testing::WithParamInterface<TestParam> {};

TEST_P(FastPairEnabledProviderTestWithParams, IsEnabledWhenExpected) {
  bool is_flag_enabled = std::get<0>(GetParam());
  bool is_bt_enabled = std::get<1>(GetParam());
  bool is_user_logged_in = std::get<2>(GetParam());
  bool is_google_api_keys_available = std::get<3>(GetParam());

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(features::kFastPair, is_flag_enabled);

  auto* bluetooth_enabled_provider = new MockBluetoothEnabledProvider();
  ON_CALL(*bluetooth_enabled_provider, is_enabled)
      .WillByDefault(testing::Return(is_bt_enabled));

  auto* logged_in_user_enabled_provider = new MockLoggedInUserEnabledProvider();
  ON_CALL(*logged_in_user_enabled_provider, is_enabled)
      .WillByDefault(testing::Return(is_user_logged_in));

  auto* google_api_key_availability_provider =
      new MockGoogleApiKeyAvailabilityProvider();
  ON_CALL(*google_api_key_availability_provider, is_enabled)
      .WillByDefault(testing::Return(is_google_api_keys_available));

  auto provider = std::make_unique<FastPairEnabledProvider>(
      std::unique_ptr<BluetoothEnabledProvider>(bluetooth_enabled_provider),
      std::unique_ptr<LoggedInUserEnabledProvider>(
          logged_in_user_enabled_provider),
      base::WrapUnique(google_api_key_availability_provider));

  bool all_are_enabled = is_flag_enabled && is_bt_enabled &&
                         is_user_logged_in && is_google_api_keys_available;

  EXPECT_EQ(provider->is_enabled(), all_are_enabled);
}

INSTANTIATE_TEST_SUITE_P(FastPairEnabledProviderTestWithParams,
                         FastPairEnabledProviderTestWithParams,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

}  // namespace quick_pair
}  // namespace ash
