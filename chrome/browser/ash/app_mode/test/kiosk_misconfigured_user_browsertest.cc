// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string_view>
#include <variant>

#include "base/notreached.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_directory_integrity_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace ash {

using kiosk::test::AutoLaunchKioskApp;
using kiosk::test::CreateDeviceLocalAccountId;
using kiosk::test::CurrentProfile;
using kiosk::test::IsAppInstalled;
using kiosk::test::WaitKioskLaunched;

namespace {

std::string_view GetAccountId(const KioskMixin::Option& option) {
  return std::visit(
      absl::Overload{
          [](const KioskMixin::DefaultServerWebAppOption& option) {
            return std::string_view(option.account_id);
          },
          [](const KioskMixin::WebAppOption& option) {
            return std::string_view(option.account_id);
          },
          [](const KioskMixin::CwsChromeAppOption& option) {
            return std::string_view(option.account_id);
          },
          [](const KioskMixin::SelfHostedChromeAppOption& option) {
            return std::string_view(option.account_id);
          },
          [](const KioskMixin::IsolatedWebAppOption& option) {
            return std::string_view(option.account_id);
          },
      },
      option);
}

policy::DeviceLocalAccountType GetAccountType(
    const KioskMixin::Option& option) {
  return std::visit(
      absl::Overload{
          [](const KioskMixin::DefaultServerWebAppOption& option) {
            return policy::DeviceLocalAccountType::kWebKioskApp;
          },
          [](const KioskMixin::WebAppOption& option) {
            return policy::DeviceLocalAccountType::kWebKioskApp;
          },
          [](const KioskMixin::CwsChromeAppOption& option) {
            return policy::DeviceLocalAccountType::kKioskApp;
          },
          [](const KioskMixin::SelfHostedChromeAppOption& option) {
            return policy::DeviceLocalAccountType::kKioskApp;
          },
          [](const KioskMixin::IsolatedWebAppOption& option) {
            return policy::DeviceLocalAccountType::kKioskIsolatedWebApp;
          },
      },
      option);
}

std::optional<AccountId> GetAutoLaunchAccountIdFromConfig(
    const KioskMixin::Config& config) {
  if (!config.auto_launch_account_id.has_value()) {
    return std::nullopt;
  }

  for (const auto& option : config.options) {
    const std::string_view account_id = GetAccountId(option);
    if (account_id != *config.auto_launch_account_id.value()) {
      continue;
    }

    return CreateDeviceLocalAccountId(account_id, GetAccountType(option));
  }

  NOTREACHED() << "Auto launch app is not found in config options.";
}

}  // namespace

// Verifies generic launch with kiosk user recorded as misconfigured user
// (e.g. cryptohome mount failure in the previous run).
// See https://crbug.com/403354377.
class KioskMisconfiguredUserTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<KioskMixin::Config> {
 public:
  KioskMisconfiguredUserTest() = default;
  KioskMisconfiguredUserTest(const KioskMisconfiguredUserTest&) = delete;
  KioskMisconfiguredUserTest& operator=(const KioskMisconfiguredUserTest&) =
      delete;

  ~KioskMisconfiguredUserTest() override = default;

  void SetUpLocalStatePrefService(PrefService* local_state) override {
    MixinBasedInProcessBrowserTest::SetUpLocalStatePrefService(local_state);

    user_manager::UserDirectoryIntegrityManager(local_state)
        .RecordCreatingNewUser(
            GetAutoLaunchAccountIdFromConfig(GetParam()).value(),
            user_manager::UserDirectoryIntegrityManager::CleanupStrategy::
                kRemoveUser);
  }

  KioskMixin kiosk_{&mixin_host_,
                    /*cached_configuration=*/GetParam()};
};

IN_PROC_BROWSER_TEST_P(KioskMisconfiguredUserTest, LaunchesAndInstallsApp) {
  ASSERT_TRUE(WaitKioskLaunched());
  ASSERT_TRUE(IsAppInstalled(CurrentProfile(), AutoLaunchKioskApp()));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskMisconfiguredUserTest,
    testing::ValuesIn(KioskMixin::ConfigsToAutoLaunchEachAppType()),
    KioskMixin::ConfigName);

}  // namespace ash
