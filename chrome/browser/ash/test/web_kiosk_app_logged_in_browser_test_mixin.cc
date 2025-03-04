// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/test/web_kiosk_app_logged_in_browser_test_mixin.h"

#include "ash/constants/ash_switches.h"
#include "base/values.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/test_helper.h"

namespace ash {

WebKioskAppLoggedInBrowserTestMixin::WebKioskAppLoggedInBrowserTestMixin(
    InProcessBrowserTestMixinHost* host,
    std::string_view account_id)
    : InProcessBrowserTestMixin(host),
      user_id_(policy::GenerateDeviceLocalAccountUserId(
          account_id,
          policy::DeviceLocalAccountType::kWebKioskApp)) {
  scoped_testing_cros_settings_.device_settings()->Set(
      ash::kAccountsPrefDeviceLocalAccounts,
      base::Value(base::Value::List().Append(
          base::Value::Dict()
              .Set(ash::kAccountsPrefDeviceLocalAccountsKeyId, account_id)
              .Set(ash::kAccountsPrefDeviceLocalAccountsKeyType,
                   static_cast<int>(
                       policy::DeviceLocalAccountType::kWebKioskApp))
              .Set(ash::kAccountsPrefDeviceLocalAccountsKeyWebKioskUrl,
                   "https://fake.web.kiosk.app.url")
              .Set(ash::kAccountsPrefDeviceLocalAccountsKeyWebKioskTitle,
                   "fake-web-kiosk-app-title")
              .Set(ash::kAccountsPrefDeviceLocalAccountsKeyWebKioskIconUrl,
                   "fake-web-kiosk-app-icon-url"))));
}

WebKioskAppLoggedInBrowserTestMixin::~WebKioskAppLoggedInBrowserTestMixin() =
    default;

void WebKioskAppLoggedInBrowserTestMixin::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(ash::switches::kLoginUser, user_id_);
  command_line->AppendSwitchASCII(ash::switches::kLoginProfile,
                                  user_manager::TestHelper::GetFakeUsernameHash(
                                      AccountId::FromUserEmail(user_id_)));

  // Do not automatically start the kiosk app.
  command_line->AppendSwitch(ash::switches::kPreventKioskAutolaunchForTesting);
}

void WebKioskAppLoggedInBrowserTestMixin::SetUpLocalStatePrefService(
    PrefService* local_state) {
  user_manager::TestHelper::RegisterWebKioskAppUser(*local_state, user_id_);
}

}  // namespace ash
