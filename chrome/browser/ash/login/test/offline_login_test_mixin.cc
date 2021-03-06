// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/offline_login_test_mixin.h"

#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/login_display_host_webui.h"
#include "chrome/browser/ash/settings/device_settings_provider.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/login/auth/user_context.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "content/public/test/test_utils.h"

namespace chromeos {

namespace {

const test::UIPath kOfflineLoginLink = {"error-message",
                                        "error-offline-login-link"};

const char kOfflineLoginDialog[] = "offline-login";

const test::UIPath kEmailPage = {kOfflineLoginDialog, "email-section"};
const test::UIPath kPasswordPage = {kOfflineLoginDialog, "password-section"};
const test::UIPath kEmailInput = {kOfflineLoginDialog, "emailInput"};
const test::UIPath kPasswordInput = {kOfflineLoginDialog, "passwordInput"};
const test::UIPath kNextButton = {kOfflineLoginDialog, "nextButton"};
const test::UIPath kManagementDisclosure = {kOfflineLoginDialog, "managedBy"};

void SetExpectedCredentials(const AccountId& test_account_id,
                            const std::string& password) {
  UserContext user_context(user_manager::UserType::USER_TYPE_REGULAR,
                           test_account_id);
  user_context.SetKey(Key(password));
  user_context.SetIsUsingOAuth(false);
  test::UserSessionManagerTestApi session_manager_test_api(
      UserSessionManager::GetInstance());
  session_manager_test_api.InjectStubUserContext(user_context);
}

}  // anonymous namespace

OfflineLoginTestMixin::OfflineLoginTestMixin(
    InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {}

OfflineLoginTestMixin::~OfflineLoginTestMixin() = default;

void OfflineLoginTestMixin::SetUpOnMainThread() {
  LoginDisplayHostWebUI::DisableRestrictiveProxyCheckForTest();
}

void OfflineLoginTestMixin::TearDownOnMainThread() {
  GoOnline();
}

void OfflineLoginTestMixin::PrepareOfflineLogin() {
  StartupUtils::MarkOobeCompleted();
  DeviceSettingsProvider(CrosSettingsProvider::NotifyObserversCallback(),
                         DeviceSettingsService::Get(),
                         g_browser_process->local_state())
      .DoSetForTesting(kAccountsPrefShowUserNamesOnSignIn, base::Value(false));
}

void OfflineLoginTestMixin::GoOffline() {
  network_state_test_helper_ =
      std::make_unique<chromeos::NetworkStateTestHelper>(
          false /*use_default_devices_and_services*/);
  network_state_test_helper_->ClearServices();
  // Notify NetworkStateInformer explicitly
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->network_state_informer_for_test()
      ->DefaultNetworkChanged(nullptr /* network */);
}

void OfflineLoginTestMixin::GoOnline() {
  network_state_test_helper_.reset();
}

void OfflineLoginTestMixin::CheckManagedStatus(bool expected_is_managed) {
  if (expected_is_managed) {
    test::OobeJS().ExpectVisiblePath(kManagementDisclosure);
  } else {
    test::OobeJS().ExpectHiddenPath(kManagementDisclosure);
  }
}

void OfflineLoginTestMixin::InitOfflineLogin(const AccountId& test_account_id,
                                             const std::string& password) {
  bool show_user;
  ASSERT_TRUE(CrosSettings::Get()->GetBoolean(
      kAccountsPrefShowUserNamesOnSignIn, &show_user));
  ASSERT_FALSE(show_user);

  StartLoginAuthOffline();

  SetExpectedCredentials(test_account_id, password);
}

void OfflineLoginTestMixin::StartLoginAuthOffline() {
  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
  test::OobeJS().TapLinkOnPath(kOfflineLoginLink);
  OobeScreenWaiter(OfflineLoginView::kScreenId).Wait();
}

void OfflineLoginTestMixin::SubmitLoginAuthOfflineForm(
    const std::string& user_email,
    const std::string& password,
    bool wait_for_signin) {
  test::OobeJS().ExpectVisible(kOfflineLoginDialog);

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
