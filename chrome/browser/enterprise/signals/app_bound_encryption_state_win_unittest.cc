// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/app_bound_encryption_state_win.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/signals/app_bound_encryption_test_utils_win.h"
#include "chrome/browser/os_crypt/app_bound_encryption_provider_win.h"
#include "chrome/browser/os_crypt/app_bound_encryption_win.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/os_crypt/async/browser/key_provider.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device_signals {

class AppBoundEncryptionStateWinTest : public testing::Test {
 protected:
  AppBoundEncryptionState FetchState(
      os_crypt_async::OSCryptAsync* os_crypt_async) {
    base::test::TestFuture<AppBoundEncryptionState> future;
    GetAppBoundEncryptionState(os_crypt_async, &local_state_,
                               future.GetCallback());
    return future.Get();
  }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
  test::ScopedAppBoundEncryptionOverrides app_bound_overrides_;
};

TEST_F(AppBoundEncryptionStateWinTest, MapSupportLevelsWhenV20Inactive) {
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async =
      test::CreateFakeOSCryptAsync("v10");

  static constexpr struct {
    os_crypt::SupportLevel support_level;
    AppBoundEncryptionState expected_state;
  } kTestCases[] = {
      {os_crypt::SupportLevel::kDisabledByPolicy,
       AppBoundEncryptionState::kDisabledByPolicy},
      {os_crypt::SupportLevel::kDisabledByRoamingWindowsProfile,
       AppBoundEncryptionState::kUnsupportedRoamingProfile},
      {os_crypt::SupportLevel::kDisabledByRoamingChromeProfile,
       AppBoundEncryptionState::kUnsupportedRoamingProfile},
      {os_crypt::SupportLevel::kNotSystemLevel,
       AppBoundEncryptionState::kUnsupportedInstallOrPath},
      {os_crypt::SupportLevel::kNotLocalDisk,
       AppBoundEncryptionState::kUnsupportedInstallOrPath},
      {os_crypt::SupportLevel::kNotUsingDefaultUserDataDir,
       AppBoundEncryptionState::kUnsupportedInstallOrPath},
      {os_crypt::SupportLevel::kUserDataDirNotLocalDisk,
       AppBoundEncryptionState::kUnsupportedInstallOrPath},
      {os_crypt::SupportLevel::kApiFailed,
       AppBoundEncryptionState::kServiceFailure},
      {os_crypt::SupportLevel::kSupported,
       AppBoundEncryptionState::kServiceFailure},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(static_cast<int>(test_case.support_level));
    app_bound_overrides_.set_support_level(test_case.support_level);
    EXPECT_EQ(FetchState(os_crypt_async.get()), test_case.expected_state);
  }
}

TEST_F(AppBoundEncryptionStateWinTest, EnabledWhenV20ProviderActive) {
  app_bound_overrides_.set_support_level(os_crypt::SupportLevel::kSupported);
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async =
      test::CreateFakeOSCryptAsync(
          std::string(os_crypt_async::kAppBoundDataPrefix));

  EXPECT_EQ(FetchState(os_crypt_async.get()),
            AppBoundEncryptionState::kEnabled);
}

TEST_F(AppBoundEncryptionStateWinTest,
       EnabledWhenV20ActiveDespitePolicyDisabled) {
  app_bound_overrides_.set_support_level(
      os_crypt::SupportLevel::kDisabledByPolicy);
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async =
      test::CreateFakeOSCryptAsync(
          std::string(os_crypt_async::kAppBoundDataPrefix),
          /*use_for_encryption=*/true);

  EXPECT_EQ(FetchState(os_crypt_async.get()),
            AppBoundEncryptionState::kEnabled);
}

TEST_F(AppBoundEncryptionStateWinTest,
       ServiceFailureWhenV20NotUsedForEncryption) {
  app_bound_overrides_.set_support_level(os_crypt::SupportLevel::kSupported);
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async =
      test::CreateFakeOSCryptAsync(
          std::string(os_crypt_async::kAppBoundDataPrefix),
          /*use_for_encryption=*/false);

  EXPECT_EQ(FetchState(os_crypt_async.get()),
            AppBoundEncryptionState::kServiceFailure);
}

TEST_F(AppBoundEncryptionStateWinTest, DeferredOSCryptAsyncCallback) {
  app_bound_overrides_.set_support_level(os_crypt::SupportLevel::kSupported);

  base::test::TestFuture<os_crypt_async::KeyProvider::KeyCallback>
      key_callback_future;
  std::vector<std::pair<os_crypt_async::OSCryptAsync::Precedence,
                        std::unique_ptr<os_crypt_async::KeyProvider>>>
      providers;
  providers.emplace_back(
      /*precedence=*/10u,
      std::make_unique<test::DeferredFakeAppBoundKeyProvider>(
          /*use_for_encryption=*/true, key_callback_future.GetCallback()));
  os_crypt_async::OSCryptAsync os_crypt_async(std::move(providers));

  base::test::TestFuture<AppBoundEncryptionState> state_future;
  GetAppBoundEncryptionState(&os_crypt_async, &local_state_,
                             state_future.GetCallback());
  EXPECT_FALSE(state_future.IsReady());
  ASSERT_TRUE(key_callback_future.Wait());

  key_callback_future.Take().Run(
      std::string(os_crypt_async::kAppBoundDataPrefix),
      test::CreateFakeAppBoundKey());

  EXPECT_EQ(state_future.Get(), AppBoundEncryptionState::kEnabled);
}

TEST_F(AppBoundEncryptionStateWinTest, SkipsSupportLevelCheckWhenV20Active) {
  app_bound_overrides_.set_support_level(os_crypt::SupportLevel::kSupported);
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async =
      test::CreateFakeOSCryptAsync(
          std::string(os_crypt_async::kAppBoundDataPrefix));

  EXPECT_EQ(FetchState(os_crypt_async.get()),
            AppBoundEncryptionState::kEnabled);
  EXPECT_EQ(app_bound_overrides_.support_level_call_count(), 0);
}

}  // namespace device_signals
