// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/check_deref.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/fake_eula_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/consolidated_consent_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/hardware_data_collection_screen_handler.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

const test::UIPath kAcceptHWDataCollectionButton = {"hw-data-collection",
                                                    "acceptButton"};
const test::UIPath kAcceptConsolidatedConsentButton = {"consolidated-consent",
                                                       "acceptButton"};
const test::UIPath kConsolidatedConsentDialog = {"consolidated-consent",
                                                 "loadedDialog"};

}  // namespace

class LoginAfterUpdateToFlexTest : public LoginManagerTest,
                                   public LocalStateMixin::Delegate {
 public:
  LoginAfterUpdateToFlexTest() { login_manager_mixin_.AppendRegularUsers(2); }

  LoginAfterUpdateToFlexTest(const LoginAfterUpdateToFlexTest&) = delete;
  LoginAfterUpdateToFlexTest& operator=(const LoginAfterUpdateToFlexTest&) =
      delete;
  ~LoginAfterUpdateToFlexTest() override = default;

  // LoginManagerTest:
  void SetUp() override {
    LoginManagerTest::SetUp();

    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kRevenBranding);
    LoginManagerTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    GetFakeUserManager().SetOwnerId(GetOwnerAccountId());
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;
  }

  void TearDownOnMainThread() override {
    LoginManagerTest::TearDownOnMainThread();
  }

  // LocalStateMixin::Delegate:
  void SetUpLocalState() override {
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetBoolean(prefs::kOobeRevenUpdatedToFlex, true);
  }

  ash::FakeChromeUserManager& GetFakeUserManager() {
    return CHECK_DEREF(static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get()));
  }

  const AccountId& GetOwnerAccountId() {
    return login_manager_mixin_.users()[0].account_id;
  }

  const AccountId& GetRegularAccountId() {
    return login_manager_mixin_.users()[1].account_id;
  }

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  FakeEulaMixin fake_eula_{&mixin_host_, embedded_test_server()};
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

IN_PROC_BROWSER_TEST_F(LoginAfterUpdateToFlexTest, DeviceOwner) {
  LoginUser(GetOwnerAccountId());
  EXPECT_FALSE(ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
      prefs::kRevenOobeConsolidatedConsentAccepted));
  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  test::OobeJS()
      .CreateVisibilityWaiter(true, kConsolidatedConsentDialog)
      ->Wait();
  test::OobeJS().TapOnPath(kAcceptConsolidatedConsentButton);
  EXPECT_TRUE(ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
      prefs::kRevenOobeConsolidatedConsentAccepted));

  OobeScreenWaiter(HWDataCollectionView::kScreenId).Wait();
  ash::test::TapOnPathAndWaitForOobeToBeDestroyed(
      kAcceptHWDataCollectionButton);
  test::WaitForPrimaryUserSessionStart();
}

IN_PROC_BROWSER_TEST_F(LoginAfterUpdateToFlexTest, RegularUser) {
  LoginUser(GetRegularAccountId());
  EXPECT_FALSE(ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
      prefs::kRevenOobeConsolidatedConsentAccepted));
  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  test::OobeJS()
      .CreateVisibilityWaiter(true, kConsolidatedConsentDialog)
      ->Wait();
  ash::test::TapOnPathAndWaitForOobeToBeDestroyed(
      kAcceptConsolidatedConsentButton);
  EXPECT_TRUE(ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
      prefs::kRevenOobeConsolidatedConsentAccepted));
  test::WaitForPrimaryUserSessionStart();
}

}  // namespace ash
