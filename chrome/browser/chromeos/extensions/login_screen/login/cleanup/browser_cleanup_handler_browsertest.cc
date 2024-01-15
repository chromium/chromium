// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/browser_cleanup_handler.h"

#include <memory>
#include <string>

#include "ash/constants/ash_switches.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/window_open_disposition.h"

namespace {

const char kAccountId[] = "public-session@test";

}  // namespace

class BrowserCleanupHandlerTest : public policy::DevicePolicyCrosBrowserTest {
 public:
  BrowserCleanupHandlerTest(const BrowserCleanupHandlerTest&) = delete;
  BrowserCleanupHandlerTest& operator=(const BrowserCleanupHandlerTest&) =
      delete;

 protected:
  BrowserCleanupHandlerTest() = default;
  ~BrowserCleanupHandlerTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);

    command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile, "user");
    command_line->AppendSwitch(ash::switches::kOobeSkipPostLogin);
  }

  void SetUpDeviceLocalAccountPolicy() {
    enterprise_management::ChromeDeviceSettingsProto& proto(
        device_policy()->payload());
    enterprise_management::DeviceLocalAccountsProto* device_local_accounts =
        proto.mutable_device_local_accounts();
    enterprise_management::DeviceLocalAccountInfoProto* const account =
        device_local_accounts->add_account();
    account->set_account_id(kAccountId);
    account->set_type(enterprise_management::DeviceLocalAccountInfoProto::
                          ACCOUNT_TYPE_PUBLIC_SESSION);
    device_local_accounts->set_auto_login_id(kAccountId);
    device_local_accounts->set_auto_login_delay(0);
    RefreshDevicePolicy();
    policy_test_server_mixin_.UpdateDevicePolicy(proto);
  }

  void WaitForSessionStart() {
    if (session_manager::SessionManager::Get()->IsSessionStarted())
      return;
    ash::test::WaitForPrimaryUserSessionStart();
  }

  Profile* GetActiveUserProfile() {
    const user_manager::User* active_user =
        user_manager::UserManager::Get()->GetActiveUser();
    return ash::ProfileHelper::Get()->GetProfileByUser(active_user);
  }

  void OpenNewBrowserPage(std::string page, WindowOpenDisposition disposition) {
    GURL page_url = embedded_test_server()->GetURL(page);

    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), page_url, disposition,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  }

  int GetHistorySize() {
    ui_test_utils::HistoryEnumerator enumerator(GetActiveUserProfile());
    return enumerator.urls().size();
  }

  void RunBrowserCleanupHandler() {
    base::RunLoop run_loop;
    auto success_check_callback = base::BindLambdaForTesting(
        [&](const std::optional<std::string>& error) {
          EXPECT_EQ(error, std::nullopt);
          run_loop.QuitClosure().Run();
        });

    std::unique_ptr<chromeos::BrowserCleanupHandler> browser_cleanup_handler =
        std::make_unique<chromeos::BrowserCleanupHandler>();
    browser_cleanup_handler->Cleanup(success_check_callback);
    run_loop.Run();
  }

  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(BrowserCleanupHandlerTest, Cleanup) {
  SetUpDeviceLocalAccountPolicy();
  WaitForSessionStart();
  ASSERT_TRUE(embedded_test_server()->Start());

  OpenNewBrowserPage("/simple.html", WindowOpenDisposition::CURRENT_TAB);
  OpenNewBrowserPage("/iframe.html", WindowOpenDisposition::NEW_FOREGROUND_TAB);
  OpenNewBrowserPage("/beforeunload.html",
                     WindowOpenDisposition::NEW_BACKGROUND_TAB);
  OpenNewBrowserPage("/unload.html", WindowOpenDisposition::NEW_WINDOW);
  OpenNewBrowserPage("/title1.html", WindowOpenDisposition::NEW_WINDOW);

  ASSERT_EQ(3U, BrowserList::GetInstance()->size());
  ASSERT_EQ(5, GetHistorySize());

  RunBrowserCleanupHandler();

  ASSERT_TRUE(BrowserList::GetInstance()->empty());
  ASSERT_EQ(0, GetHistorySize());
}

IN_PROC_BROWSER_TEST_F(BrowserCleanupHandlerTest, CleanupWhenBrowsersClosed) {
  SetUpDeviceLocalAccountPolicy();
  WaitForSessionStart();
  ASSERT_TRUE(embedded_test_server()->Start());

  OpenNewBrowserPage("/simple.html", WindowOpenDisposition::CURRENT_TAB);
  BrowserList::CloseAllBrowsersWithProfile(
      GetActiveUserProfile(),
      /*on_close_success=*/BrowserList::CloseCallback(),
      /*on_close_aborted=*/BrowserList::CloseCallback(),
      /*skip_beforeunload=*/true);
  ui_test_utils::WaitForBrowserToClose();

  ASSERT_TRUE(BrowserList::GetInstance()->empty());
  ASSERT_EQ(1, GetHistorySize());

  RunBrowserCleanupHandler();

  ASSERT_TRUE(BrowserList::GetInstance()->empty());
  ASSERT_EQ(0, GetHistorySize());
}
