// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/account_manager/fake_account_manager_ui_dialog_waiter.h"
#endif

// Tests the behavior of Chrome when it receives a Mirror response from Gaia:
// - listens to all network responses coming from Gaia with
//   `signin::HeaderModificationDelegate`.
// - parses the Mirror response header with
// `signin::BuildManageAccountsParams()`
// - triggers dialogs based on the action specified in the header, with
//   `ProcessMirrorHeader`
// The tests don't display real dialogs. Instead they use the
// `FakeAccountManagerUI` and only check that the dialogs were triggered.
// The tests are interactive_ui_tests because they depend on browser's window
// activation state.
class MirrorResponseBrowserTest : public InProcessBrowserTest {
 public:
  MirrorResponseBrowserTest(const MirrorResponseBrowserTest&) = delete;
  MirrorResponseBrowserTest& operator=(const MirrorResponseBrowserTest&) =
      delete;

 protected:
  ~MirrorResponseBrowserTest() override = default;

  MirrorResponseBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  // Navigates to Gaia and receives a response with the specified
  // "X-Chrome-Manage-Accounts" header.
  void ReceiveManageAccountsHeader(
      const base::flat_map<std::string, std::string>& header_params) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GetUrlWithManageAccountsHeader(header_params)));
  }

  GURL GetUrlWithManageAccountsHeader(
      const base::flat_map<std::string, std::string>& header_params) {
    std::vector<std::string> parts;
    for (const auto& [key, value] : header_params) {
      // "=" must be escaped as "%3D" for the embedded server.
      const char kEscapedEquals[] = "%3D";
      parts.push_back(key + kEscapedEquals + value);
    }
    std::string path = std::string("/set-header?X-Chrome-Manage-Accounts: ") +
                       base::JoinString(parts, ",");
    return https_server_.GetURL(path);
  }

  // InProcessBrowserTest:
  void SetUp() override {
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    const GURL& base_url = https_server_.base_url();
    command_line->AppendSwitchASCII(switches::kGaiaUrl, base_url.spec());
    command_line->AppendSwitchASCII(switches::kGoogleApisUrl, base_url.spec());
    command_line->AppendSwitchASCII(switches::kLsoUrl, base_url.spec());
  }

  void SetUpOnMainThread() override {
    https_server_.StartAcceptingConnections();
    InProcessBrowserTest::SetUpOnMainThread();
  }

  net::EmbeddedTestServer https_server_;
  net::test_server::EmbeddedTestServerHandle https_server_handle_;
};

// Following tests try to display the ChromeOS account manager dialogs. They can
// currently be tested only on Lacros which injects a `FakeAccountManagerUI`.
#if BUILDFLAG(IS_CHROMEOS_LACROS)

// Tests that the "Add Account" dialog is shown when receiving "ADDSESSION" from
// Gaia.
IN_PROC_BROWSER_TEST_F(MirrorResponseBrowserTest, AddSession) {
  FakeAccountManagerUIDialogWaiter dialog_waiter(
      GetFakeAccountManagerUI(),
      FakeAccountManagerUIDialogWaiter::Event::kAddAccount);
  ReceiveManageAccountsHeader({{"action", "ADDSESSION"}});
  dialog_waiter.Wait();
}

// Tests that the "Settings"" dialog is shown when receiving "DEFAULT" from
// Gaia.
IN_PROC_BROWSER_TEST_F(MirrorResponseBrowserTest, Settings) {
  FakeAccountManagerUIDialogWaiter dialog_waiter(
      GetFakeAccountManagerUI(),
      FakeAccountManagerUIDialogWaiter::Event::kSettings);
  ReceiveManageAccountsHeader({{"action", "DEFAULT"}});
  dialog_waiter.Wait();
}

// Tests that the "Reauth" dialog is shown when receiving an email from Gaia.
IN_PROC_BROWSER_TEST_F(MirrorResponseBrowserTest, Reauth) {
  FakeAccountManagerUIDialogWaiter dialog_waiter(
      GetFakeAccountManagerUI(),
      FakeAccountManagerUIDialogWaiter::Event::kReauth);
  ReceiveManageAccountsHeader(
      {{"action", "ADDSESSION"}, {"email", "user@example.com"}});
  dialog_waiter.Wait();
}

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

// Tests that incognito browser is opened when receiving "INCOGNITO" from Gaia.
IN_PROC_BROWSER_TEST_F(MirrorResponseBrowserTest, Incognito) {
  ui_test_utils::BrowserChangeObserver browser_change_observer(
      /*browser=*/nullptr,
      ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  ReceiveManageAccountsHeader({{"action", "INCOGNITO"}});
  Browser* incognito_browser = browser_change_observer.Wait();
  EXPECT_TRUE(incognito_browser->profile()->IsIncognitoProfile());
}

// Tests that the response is coming from a background browser is ignored.
IN_PROC_BROWSER_TEST_F(MirrorResponseBrowserTest, BackgroundResponseIgnored) {
  // Minimize the browser window to disactivate it.
  browser()->window()->Minimize();
  EXPECT_FALSE(browser()->window()->IsActive());

  size_t browser_count = chrome::GetTotalBrowserCount();
  GURL url = GetUrlWithManageAccountsHeader({{"action", "INCOGNITO"}});
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_FROM_API);
  // Use `NEW_BACKGROUND_TAB` to avoid activating `browser()`.
  params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  Navigate(&params);
  EXPECT_TRUE(content::WaitForLoadStop(params.navigated_or_inserted_contents));

  // Incognito window should not have been displayed, the browser count stays
  // the same.
  EXPECT_EQ(chrome::GetTotalBrowserCount(), browser_count);
}
