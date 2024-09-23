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
#include "base/metrics/histogram_base.h"
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
#include "content/public/test/browser_test_utils.h"
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
    NavigateToURL(GetUrlWithManageAccountsHeader(header_params), std::nullopt);
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

  // Helper method to navigate with an optional request initiator origin.
  void NavigateToURL(const GURL& url,
                     std::optional<url::Origin> initiator_origin) {
    NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
    params.disposition = WindowOpenDisposition::CURRENT_TAB;
    if (initiator_origin) {
      // `is_renderer_initiated` requires non-null `initiator_origin`.
      params.is_renderer_initiated = true;
      params.initiator_origin = initiator_origin;
    }
    Navigate(&params);
    EXPECT_TRUE(
        content::WaitForLoadStop(params.navigated_or_inserted_contents));
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

// When receiving "INCOGNITO" from Gaia and the request is initiated by a Google
// domain - an incognito tab should be opened.
IN_PROC_BROWSER_TEST_F(MirrorResponseBrowserTest, Incognito) {
  base::HistogramTester histogram_tester;
  size_t browser_count = chrome::GetTotalBrowserCount();
  ui_test_utils::BrowserChangeObserver browser_change_observer(
      /*browser=*/nullptr,
      ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);

  NavigateToURL(GetUrlWithManageAccountsHeader({{"action", "INCOGNITO"}}),
                url::Origin::Create(GURL("https://google.com")));

  // Incognito window should have been displayed, the browser count goes up.
  EXPECT_GT(chrome::GetTotalBrowserCount(), browser_count);

  // No waiting happens here - BrowserChangeObserver is used to obtain a pointer
  // to the newly added browser.
  Browser* incognito_browser = browser_change_observer.Wait();
  EXPECT_TRUE(incognito_browser->profile()->IsIncognitoProfile());

  histogram_tester.ExpectUniqueSample(
      "Signin.ProcessMirrorHeaders.AllowedFromInitiator.GoIncognito", true, 1);
}

// When receiving "INCOGNITO" from Gaia and the request is initiator is unknown
// - an incognito tab should not be opened.
IN_PROC_BROWSER_TEST_F(MirrorResponseBrowserTest,
                       IncognitoFromEmptyInitiatorIgnored) {
  base::HistogramTester histogram_tester;
  size_t browser_count = chrome::GetTotalBrowserCount();

  NavigateToURL(GetUrlWithManageAccountsHeader({{"action", "INCOGNITO"}}),
                std::nullopt);

  // Incognito window should not have been displayed, the browser count
  // stays the same.
  EXPECT_EQ(chrome::GetTotalBrowserCount(), browser_count);

  histogram_tester.ExpectUniqueSample(
      "Signin.ProcessMirrorHeaders.AllowedFromInitiator.GoIncognito", false, 1);
}

// When receiving "INCOGNITO" from Gaia and the request initiator is
// a Google-associated domain (but not Google or Youtube) - an incognito tab
// should not be opened.
IN_PROC_BROWSER_TEST_F(MirrorResponseBrowserTest,
                       IncognitoFromGoogleapisInitiatorIgnored) {
  base::HistogramTester histogram_tester;
  size_t browser_count = chrome::GetTotalBrowserCount();

  NavigateToURL(GetUrlWithManageAccountsHeader({{"action", "INCOGNITO"}}),
                url::Origin::Create(GURL("https://storage.googleapis.com")));

  // Incognito window should not have been displayed, the browser count
  // stays the same.
  EXPECT_EQ(chrome::GetTotalBrowserCount(), browser_count);

  histogram_tester.ExpectUniqueSample(
      "Signin.ProcessMirrorHeaders.AllowedFromInitiator.GoIncognito", false, 1);
}

// When receiving "INCOGNITO" from Gaia and the request initiator is not a
// Google domain - an incognito tab should not be opened.
IN_PROC_BROWSER_TEST_F(MirrorResponseBrowserTest,
                       IncognitoFromNonGoogleInitiatorIgnored) {
  base::HistogramTester histogram_tester;
  size_t browser_count = chrome::GetTotalBrowserCount();

  NavigateToURL(GetUrlWithManageAccountsHeader({{"action", "INCOGNITO"}}),
                url::Origin::Create(GURL("https://example.com")));

  // Incognito window should not have been displayed, the browser count
  // stays the same.
  EXPECT_EQ(chrome::GetTotalBrowserCount(), browser_count);

  histogram_tester.ExpectUniqueSample(
      "Signin.ProcessMirrorHeaders.AllowedFromInitiator.GoIncognito", false, 1);
}

// When receiving "INCOGNITO" from Gaia in a background browser - an incognito
// tab should not be opened.
IN_PROC_BROWSER_TEST_F(MirrorResponseBrowserTest, BackgroundResponseIgnored) {
  // Minimize the browser window to disactivate it.
  browser()->window()->Minimize();
  ASSERT_TRUE(ui_test_utils::WaitForMinimized(browser()));
  EXPECT_FALSE(browser()->window()->IsActive());

  size_t browser_count = chrome::GetTotalBrowserCount();
  GURL url = GetUrlWithManageAccountsHeader({{"action", "INCOGNITO"}});
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_FROM_API);
  params.initiator_origin = url::Origin::Create(GURL("https://google.com"));
  // Use `NEW_BACKGROUND_TAB` to avoid activating `browser()`.
  params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  params.is_renderer_initiated = true;
  Navigate(&params);
  EXPECT_TRUE(content::WaitForLoadStop(params.navigated_or_inserted_contents));

  // Incognito window should not have been displayed, the browser count stays
  // the same.
  EXPECT_EQ(chrome::GetTotalBrowserCount(), browser_count);
}
