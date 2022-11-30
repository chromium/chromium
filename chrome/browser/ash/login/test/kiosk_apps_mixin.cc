// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/kiosk_apps_mixin.h"

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/app_mode/fake_cws.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace ash {

// This is a simple test app that creates an app window and immediately closes
// it again. Webstore data json is in
//   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/
//       detail/ggaeimfdpnmlhdhpcikgoblffmkckdmn
constexpr char KioskAppsMixin::kKioskAppId[] =
    "ggaeimfdpnmlhdhpcikgoblffmkckdmn";

constexpr char KioskAppsMixin::kEnterpriseKioskAccountId[] =
    "enterprise-kiosk-app@localhost";
constexpr char KioskAppsMixin::kEnterpriseWebKioskAccountId[] = "web_kiosk_id";

// static
void KioskAppsMixin::WaitForAppsButton() {
  while (!LoginScreenTestApi::IsAppsButtonShown()) {
    int ui_update_count = LoginScreenTestApi::GetUiUpdateCount();
    LoginScreenTestApi::WaitForUiUpdate(ui_update_count);
  }
}

// static
void KioskAppsMixin::AppendKioskAccount(
    enterprise_management::ChromeDeviceSettingsProto* policy_payload) {
  enterprise_management::DeviceLocalAccountsProto* const device_local_accounts =
      policy_payload->mutable_device_local_accounts();

  enterprise_management::DeviceLocalAccountInfoProto* const account =
      device_local_accounts->add_account();
  account->set_account_id(KioskAppsMixin::kEnterpriseKioskAccountId);
  account->set_type(enterprise_management::DeviceLocalAccountInfoProto::
                        ACCOUNT_TYPE_KIOSK_APP);
  account->mutable_kiosk_app()->set_app_id(KioskAppsMixin::kKioskAppId);
}

// static
void KioskAppsMixin::AppendWebKioskAccount(
    enterprise_management::ChromeDeviceSettingsProto* policy_payload) {
  enterprise_management::DeviceLocalAccountsProto* const device_local_accounts =
      policy_payload->mutable_device_local_accounts();

  enterprise_management::DeviceLocalAccountInfoProto* const account =
      device_local_accounts->add_account();
  account->set_account_id(KioskAppsMixin::kEnterpriseWebKioskAccountId);
  account->set_type(enterprise_management::DeviceLocalAccountInfoProto::
                        ACCOUNT_TYPE_WEB_KIOSK_APP);
  account->mutable_web_kiosk_app()->set_url("https://example.com");
}

// static
void KioskAppsMixin::AppendAutoLaunchKioskAccount(
    enterprise_management::ChromeDeviceSettingsProto* policy_payload) {
  enterprise_management::DeviceLocalAccountsProto* const device_local_accounts =
      policy_payload->mutable_device_local_accounts();

  enterprise_management::DeviceLocalAccountInfoProto* const account =
      device_local_accounts->add_account();
  account->set_account_id(KioskAppsMixin::kEnterpriseKioskAccountId);
  account->set_type(enterprise_management::DeviceLocalAccountInfoProto::
                        ACCOUNT_TYPE_KIOSK_APP);
  account->mutable_kiosk_app()->set_app_id(KioskAppsMixin::kKioskAppId);
  device_local_accounts->set_auto_login_id(
      KioskAppsMixin::kEnterpriseKioskAccountId);
}

KioskAppsMixin::KioskAppsMixin(InProcessBrowserTestMixinHost* host,
                               net::EmbeddedTestServer* embedded_test_server)
    : InProcessBrowserTestMixin(host),
      embedded_test_server_(embedded_test_server) {}

KioskAppsMixin::~KioskAppsMixin() = default;

void KioskAppsMixin::SetUpInProcessBrowserTestFixture() {
  fake_cws_.Init(embedded_test_server_);
  fake_cws_.SetUpdateCrx(kKioskAppId, base::StrCat({kKioskAppId, ".crx"}),
                         "1.0.0");
}

}  // namespace ash
