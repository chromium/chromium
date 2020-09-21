// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/offline_gaia_test_mixin.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/session/user_session_manager_test_api.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/session_manager_state_waiter.h"
#include "chrome/browser/chromeos/login/test/test_condition_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_webui.h"
#include "chrome/browser/chromeos/settings/device_settings_provider.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/login/auth/user_context.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "content/public/test/test_utils.h"

namespace chromeos {

namespace {

const test::UIPath kOfflineLoginLink = {"error-offline-login-link"};

const test::UIPath kOfflineGaiaDialog = {"gaia-signin", "offline-gaia"};
const test::UIPath kOnlineGaiaDialog = {"gaia-signin", "signin-frame-dialog"};

const test::UIPath kEmailPage = {"gaia-signin", "offline-gaia",
                                 "email-section"};
const test::UIPath kPasswordPage = {"gaia-signin", "offline-gaia",
                                    "password-section"};
const test::UIPath kEmailInput = {"gaia-signin", "offline-gaia", "emailInput"};
const test::UIPath kPasswordInput = {"gaia-signin", "offline-gaia",
                                     "passwordInput"};
const test::UIPath kNextButton = {"gaia-signin", "offline-gaia", "next-button"};
const test::UIPath kManagementDisclosure = {"gaia-signin", "offline-gaia",
                                            "managedBy"};

void SetExpectedCredentials(const AccountId& test_account_id,
                            const std::string& password) {
  UserContext user_context(user_manager::UserType::USER_TYPE_REGULAR,
                           test_account_id);
  user_context.SetKey(Key(password));
  test::UserSessionManagerTestApi session_manager_test_api(
      UserSessionManager::GetInstance());
  session_manager_test_api.InjectStubUserContext(user_context);
}

}  // anonymous namespace

OfflineGaiaTestMixin::OfflineGaiaTestMixin(InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {}

OfflineGaiaTestMixin::~OfflineGaiaTestMixin() = default;

void OfflineGaiaTestMixin::SetUpOnMainThread() {
  LoginDisplayHostWebUI::DisableRestrictiveProxyCheckForTest();
}

void OfflineGaiaTestMixin::TearDownOnMainThread() {
  GoOnline();
}

void OfflineGaiaTestMixin::PrepareOfflineGaiaLogin() {
  StartupUtils::MarkOobeCompleted();
  DeviceSettingsProvider(CrosSettingsProvider::NotifyObserversCallback(),
                         DeviceSettingsService::Get(),
                         g_browser_process->local_state())
      .DoSetForTesting(kAccountsPrefShowUserNamesOnSignIn, base::Value(false));
}

void OfflineGaiaTestMixin::GoOffline() {
  network_state_test_helper_ =
      std::make_unique<chromeos::NetworkStateTestHelper>(
          false /*use_default_devices_and_services*/);
  network_state_test_helper_->ClearServices();
}

void OfflineGaiaTestMixin::GoOnline() {
  network_state_test_helper_.reset();
}

void OfflineGaiaTestMixin::CheckManagedStatus(bool expected_is_managed) {
  if (expected_is_managed) {
    test::OobeJS().ExpectVisiblePath(kManagementDisclosure);
  } else {
    test::OobeJS().ExpectHiddenPath(kManagementDisclosure);
  }
}

void OfflineGaiaTestMixin::InitOfflineLogin(const AccountId& test_account_id,
                                            const std::string& password) {
  bool show_user;
  ASSERT_TRUE(CrosSettings::Get()->GetBoolean(
      kAccountsPrefShowUserNamesOnSignIn, &show_user));
  ASSERT_FALSE(show_user);

  StartGaiaAuthOffline();

  SetExpectedCredentials(test_account_id, password);
}

void OfflineGaiaTestMixin::StartGaiaAuthOffline() {
  test::OobeJS()
      .CreateWaiter("window.$ && $('error-offline-login-link')")
      ->Wait();
  test::OobeJS().TapLinkOnPath(kOfflineLoginLink);
  test::OobeJS().CreateVisibilityWaiter(true, kOfflineGaiaDialog)->Wait();
}

void OfflineGaiaTestMixin::SubmitGaiaAuthOfflineForm(
    const std::string& user_email,
    const std::string& password,
    bool wait_for_signin) {
  test::OobeJS().ExpectVisiblePath(kOfflineGaiaDialog);
  test::OobeJS().ExpectHiddenPath(kOnlineGaiaDialog);

  test::OobeJS().CreateDisplayedWaiter(true, kEmailPage)->Wait();
  test::OobeJS().CreateDisplayedWaiter(false, kPasswordPage)->Wait();

  test::OobeJS().TypeIntoPath(user_email, kEmailInput);

  test::OobeJS().ClickOnPath(kNextButton);

  test::OobeJS().CreateDisplayedWaiter(false, kEmailPage)->Wait();
  test::OobeJS().CreateDisplayedWaiter(true, kPasswordPage)->Wait();

  test::OobeJS().TypeIntoPath(password, kPasswordInput);

  test::OobeJS().ClickOnPath(kNextButton);

  if (wait_for_signin) {
    SessionStateWaiter(session_manager::SessionState::LOGGED_IN_NOT_ACTIVE)
        .Wait();
  }
}

}  // namespace chromeos
