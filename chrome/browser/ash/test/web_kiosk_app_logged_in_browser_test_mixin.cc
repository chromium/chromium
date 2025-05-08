// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/test/web_kiosk_app_logged_in_browser_test_mixin.h"

#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/values.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "chromeos/ash/components/settings/device_settings_cache_test_support.h"
#include "components/account_id/account_id.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/test_helper.h"

namespace em = enterprise_management;

namespace ash {

WebKioskAppLoggedInBrowserTestMixin::WebKioskAppLoggedInBrowserTestMixin(
    InProcessBrowserTestMixinHost* host,
    std::string account_id)
    : InProcessBrowserTestMixin(host),
      account_id_(std::move(account_id)),
      user_id_(policy::GenerateDeviceLocalAccountUserId(
          account_id_,
          policy::DeviceLocalAccountType::kWebKioskApp)) {}

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
  // Update local_state cache used in UserManager.
  user_manager::TestHelper::RegisterWebKioskAppUser(*local_state, user_id_);

  // Update device settings cache.
  CHECK(device_settings_cache::Update(
      local_state, [&](em::PolicyData& policy_data) {
        em::ChromeDeviceSettingsProto settings;
        if (policy_data.has_policy_value()) {
          CHECK(settings.ParseFromString(policy_data.policy_value()));
        }

        auto* account = settings.mutable_device_local_accounts()->add_account();
        account->set_account_id(account_id_);
        account->set_type(
            em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_WEB_KIOSK_APP);
        account->mutable_web_kiosk_app()->set_url(
            "https://fake.web.kiosk.app.url");
        account->mutable_web_kiosk_app()->set_title("fake-web-kiosk-app-title");
        account->mutable_web_kiosk_app()->set_icon_url(
            "fake-web-kiosk-app-icon-url");

        policy_data.set_policy_value(settings.SerializeAsString());
      }));
}

}  // namespace ash
