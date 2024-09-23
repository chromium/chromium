// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/test/kiosk_apps_mixin.h"

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/app_mode/fake_cws.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace ash {

namespace {

using enterprise_management::DeviceLocalAccountInfoProto;
using enterprise_management::DeviceLocalAccountsProto;

}  // namespace

// static
void KioskAppsMixin::WaitForAppsButton() {
  while (!LoginScreenTestApi::IsAppsButtonShown()) {
    int ui_update_count = LoginScreenTestApi::GetUiUpdateCount();
    LoginScreenTestApi::WaitForUiUpdate(ui_update_count);
  }
}

// static
void KioskAppsMixin::AppendKioskAccount(
    enterprise_management::ChromeDeviceSettingsProto* policy_payload,
    std::string_view app_id,
    std::string_view account_id) {
  DeviceLocalAccountInfoProto* account =
      policy_payload->mutable_device_local_accounts()->add_account();

  account->set_account_id(std::string(account_id));
  account->set_type(DeviceLocalAccountInfoProto::ACCOUNT_TYPE_KIOSK_APP);
  account->mutable_kiosk_app()->set_app_id(std::string(app_id));
}

// static
void KioskAppsMixin::AppendAutoLaunchKioskAccount(
    enterprise_management::ChromeDeviceSettingsProto* policy_payload,
    std::string_view app_id,
    std::string_view account_id) {
  AppendKioskAccount(policy_payload, app_id, account_id);

  policy_payload->mutable_device_local_accounts()->set_auto_login_id(
      std::string(account_id));
}

// static
void KioskAppsMixin::AppendWebKioskAccount(
    enterprise_management::ChromeDeviceSettingsProto* policy_payload,
    std::string_view url,
    std::string_view account_id) {
  DeviceLocalAccountInfoProto* account =
      policy_payload->mutable_device_local_accounts()->add_account();

  account->set_account_id(std::string(account_id));
  account->set_type(DeviceLocalAccountInfoProto::ACCOUNT_TYPE_WEB_KIOSK_APP);
  account->mutable_web_kiosk_app()->set_url(std::string(url));
}

KioskAppsMixin::KioskAppsMixin(InProcessBrowserTestMixinHost* host,
                               net::EmbeddedTestServer* embedded_test_server)
    : InProcessBrowserTestMixin(host),
      embedded_test_server_(embedded_test_server) {}

KioskAppsMixin::~KioskAppsMixin() = default;

void KioskAppsMixin::SetUpInProcessBrowserTestFixture() {
  fake_cws_.Init(embedded_test_server_);
  fake_cws_.SetUpdateCrx(kTestChromeAppId,
                         base::StrCat({kTestChromeAppId, ".crx"}), "1.0.0");
}

}  // namespace ash
