// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "ash/webui/settings/public/constants/routes_util.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/account_manager/scoped_fake_account_manager_dialog.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/browser_delegate/browser_delegate.h"
#include "chromeos/ash/components/system_web_apps/system_web_app_type.h"
#include "components/account_manager_core/account_addition_options.h"
#include "components/account_manager_core/account_manager_metrics.h"
#include "components/account_manager_core/account_upsertion_result.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

// Tests the behavior of Chrome when it receives a Mirror response from Gaia:
// - listens to all network responses coming from Gaia with
//   `signin::HeaderModificationDelegate`.
// - parses the Mirror response header with
// `signin::BuildManageAccountsParams()`
// - triggers dialogs based on the action specified in the header, with
//   `ProcessMirrorHeader`
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

// When receiving "ADDSESSION" from Gaia, the One Google Bar add-account path
// should open the Account Manager add-account dialog.
IN_PROC_BROWSER_TEST_F(MirrorResponseBrowserTest,
                       AddSessionOpensAccountManagerDialog) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  base::HistogramTester histogram_tester;
  ash::test::ScopedFakeAccountManagerDialog fake_account_manager_dialog(
      browser()->GetProfile());

  ReceiveManageAccountsHeader({{"action", "ADDSESSION"}});

  ASSERT_TRUE(base::test::RunUntil([&fake_account_manager_dialog] {
    return fake_account_manager_dialog->show_account_addition_dialog_calls() ==
           1;
  }));
  ASSERT_TRUE(
      fake_account_manager_dialog->last_add_account_options().has_value());
  EXPECT_FALSE(fake_account_manager_dialog->last_add_account_options()
                   ->is_available_in_arc);
  EXPECT_FALSE(fake_account_manager_dialog->last_add_account_options()
                   ->show_arc_availability_picker);
  EXPECT_EQ(0, fake_account_manager_dialog
                   ->show_account_reauthentication_dialog_calls());
  histogram_tester.ExpectUniqueSample(
      account_manager::kAccountAdditionSourceHistogramName,
      account_manager::AccountAdditionSource::kOgbAddAccount,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      account_manager::kAccountUpsertionResultStatusHistogramName, 0);

  fake_account_manager_dialog->CloseDialog();

  histogram_tester.ExpectUniqueSample(
      account_manager::kAccountUpsertionResultStatusHistogramName,
      account_manager::AccountUpsertionResult::Status::kCancelledByUser,
      /*expected_count=*/1);
}

// When receiving "DEFAULT" from Gaia, Mirror should open Account Manager
// settings without recording add-account or reauth UMA.
IN_PROC_BROWSER_TEST_F(MirrorResponseBrowserTest,
                       DefaultOpensManageAccountsSettings) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  base::HistogramTester histogram_tester;
  ash::test::ScopedFakeAccountManagerDialog fake_account_manager_dialog(
      browser()->GetProfile());
  ash::SystemWebAppManager::GetForTest(browser()->GetProfile())
      ->InstallSystemAppsForTesting();

  const GURL os_settings_people = chromeos::settings::GetOSSettingsUrl(
      chromeos::settings::mojom::kPeopleSectionPath);
  content::TestNavigationObserver navigation_observer(os_settings_people);
  navigation_observer.StartWatchingNewWebContents();

  ReceiveManageAccountsHeader({{"action", "DEFAULT"}});

  navigation_observer.Wait();

  ash::BrowserDelegate* settings_browser = ash::FindSystemWebAppBrowser(
      browser()->GetProfile(), ash::SystemWebAppType::SETTINGS,
      ash::BrowserType::kApp);
  ASSERT_TRUE(settings_browser);
  content::WebContents* settings_contents =
      settings_browser->GetActiveWebContents();
  ASSERT_TRUE(settings_contents);
  EXPECT_EQ(os_settings_people, settings_contents->GetLastCommittedURL());
  EXPECT_EQ(0,
            fake_account_manager_dialog->show_account_addition_dialog_calls());
  EXPECT_EQ(0, fake_account_manager_dialog
                   ->show_account_reauthentication_dialog_calls());
  histogram_tester.ExpectTotalCount(
      account_manager::kAccountAdditionSourceHistogramName, 0);
  histogram_tester.ExpectTotalCount(
      account_manager::kAccountUpsertionResultStatusHistogramName, 0);
}

// When receiving "INCOGNITO" from Gaia and the request is initiated by a Google
// domain - an incognito tab should be opened.
IN_PROC_BROWSER_TEST_F(MirrorResponseBrowserTest, Incognito) {
  base::HistogramTester histogram_tester;
  size_t browser_count = GlobalBrowserCollection::GetInstance()->GetSize();
  ui_test_utils::BrowserCreatedObserver browser_created_observer;

  NavigateToURL(GetUrlWithManageAccountsHeader({{"action", "INCOGNITO"}}),
                url::Origin::Create(GURL("https://google.com")));

  // Incognito window should have been displayed, the browser count goes up.
  EXPECT_GT(GlobalBrowserCollection::GetInstance()->GetSize(), browser_count);

  // No waiting happens here - BrowserCreatedObserver is used to obtain a
  // pointer to the newly added browser.
  BrowserWindowInterface* incognito_browser = browser_created_observer.Wait();
  EXPECT_TRUE(incognito_browser->GetProfile()->IsIncognitoProfile());

  histogram_tester.ExpectUniqueSample(
      "Signin.ProcessMirrorHeaders.AllowedFromInitiator.GoIncognito", true, 1);
}

// When receiving "INCOGNITO" from Gaia and the request is initiator is unknown
// - an incognito tab should not be opened.
IN_PROC_BROWSER_TEST_F(MirrorResponseBrowserTest,
                       IncognitoFromEmptyInitiatorIgnored) {
  base::HistogramTester histogram_tester;
  size_t browser_count = GlobalBrowserCollection::GetInstance()->GetSize();

  NavigateToURL(GetUrlWithManageAccountsHeader({{"action", "INCOGNITO"}}),
                std::nullopt);

  // Incognito window should not have been displayed, the browser count
  // stays the same.
  EXPECT_EQ(GlobalBrowserCollection::GetInstance()->GetSize(), browser_count);

  histogram_tester.ExpectUniqueSample(
      "Signin.ProcessMirrorHeaders.AllowedFromInitiator.GoIncognito", false, 1);
}

// When receiving "INCOGNITO" from Gaia and the request initiator is
// a Google-associated domain (but not Google or Youtube) - an incognito tab
// should not be opened.
IN_PROC_BROWSER_TEST_F(MirrorResponseBrowserTest,
                       IncognitoFromGoogleapisInitiatorIgnored) {
  base::HistogramTester histogram_tester;
  size_t browser_count = GlobalBrowserCollection::GetInstance()->GetSize();

  NavigateToURL(GetUrlWithManageAccountsHeader({{"action", "INCOGNITO"}}),
                url::Origin::Create(GURL("https://storage.googleapis.com")));

  // Incognito window should not have been displayed, the browser count
  // stays the same.
  EXPECT_EQ(GlobalBrowserCollection::GetInstance()->GetSize(), browser_count);

  histogram_tester.ExpectUniqueSample(
      "Signin.ProcessMirrorHeaders.AllowedFromInitiator.GoIncognito", false, 1);
}

// When receiving "INCOGNITO" from Gaia and the request initiator is not a
// Google domain - an incognito tab should not be opened.
IN_PROC_BROWSER_TEST_F(MirrorResponseBrowserTest,
                       IncognitoFromNonGoogleInitiatorIgnored) {
  base::HistogramTester histogram_tester;
  size_t browser_count = GlobalBrowserCollection::GetInstance()->GetSize();

  NavigateToURL(GetUrlWithManageAccountsHeader({{"action", "INCOGNITO"}}),
                url::Origin::Create(GURL("https://example.com")));

  // Incognito window should not have been displayed, the browser count
  // stays the same.
  EXPECT_EQ(GlobalBrowserCollection::GetInstance()->GetSize(), browser_count);

  histogram_tester.ExpectUniqueSample(
      "Signin.ProcessMirrorHeaders.AllowedFromInitiator.GoIncognito", false, 1);
}

// When receiving "INCOGNITO" from Gaia in a background browser - an incognito
// tab should not be opened.
IN_PROC_BROWSER_TEST_F(MirrorResponseBrowserTest, BackgroundResponseIgnored) {
  // Minimize the browser window to disactivate it.
  browser()->GetWindow()->Minimize();
  ASSERT_TRUE(ui_test_utils::WaitForMinimized(browser()));
  EXPECT_FALSE(browser()->GetWindow()->IsActive());

  size_t browser_count = GlobalBrowserCollection::GetInstance()->GetSize();
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
  EXPECT_EQ(GlobalBrowserCollection::GetInstance()->GetSize(), browser_count);
}
