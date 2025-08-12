// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/ask_before_http_dialog_controller.h"
#include "chrome/browser/ssl/https_upgrades_interceptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/test_data_directory.h"
#include "ui/views/controls/styled_label.h"

// This is a subset of variations from `HttpsUpgradesTestType` that are
// particularly relevant for testing the Ask-before-HTTP native warning dialog.
// See `HttpsUpgradesBrowserTest` for more testing of the underlying
// upgrades/fallback/warning features.
enum class AskBeforeHttpDialogControllerTestType {
  // Enables the HFM pref.
  kHttpsFirstModeOnly,

  // Enables HFM in Incognito mode. Runs testcases inside an Incognito
  // window.
  kHttpsFirstModeIncognito,

  // Enables HFM in balanced mode.
  kHttpsFirstBalancedMode,

  // Enables HFM pref, HFM with Site Engagement heuristic, HFM for typically
  // secure users, HFM in incognito, and balanced HFM feature flags.
  kAll,
};

// This test suite exercises the new Ask-before-HTTP native dialog UI, while
// the test suite in https_upgrades_browsertest.cc runs with the old
// interstitial UI.
class AskBeforeHttpDialogControllerUiTest
    : public testing::WithParamInterface<AskBeforeHttpDialogControllerTestType>,
      public InteractiveBrowserTest {
 public:
  AskBeforeHttpDialogControllerUiTest() = default;
  ~AskBeforeHttpDialogControllerUiTest() override = default;

  void SetUp() override {
    // HFM is controlled by a pref (configured in SetUpOnMainThread).
    switch (test_type()) {
      case AskBeforeHttpDialogControllerTestType::kHttpsFirstModeOnly:
        feature_list_.InitWithFeatures(
            /*enabled_features=*/{features::kHttpsFirstDialogUi},
            /*disabled_features=*/{
                features::kHttpsFirstModeV2ForEngagedSites,
                features::kHttpsFirstModeV2ForTypicallySecureUsers});
        break;

      case AskBeforeHttpDialogControllerTestType::kHttpsFirstModeIncognito:
        feature_list_.InitWithFeatures(
            /*enabled_features=*/{features::kHttpsFirstDialogUi,
                                  features::kHttpsFirstModeIncognito},
            /*disabled_features=*/{});
        break;

      case AskBeforeHttpDialogControllerTestType::kHttpsFirstBalancedMode:
        feature_list_.InitWithFeatures(
            /*enabled_features=*/{features::kHttpsFirstDialogUi,
                                  features::kHttpsFirstBalancedMode,
                                  features::kHttpsFirstBalancedModeAutoEnable},
            /*disabled_features=*/{
                features::kHttpsFirstModeV2ForTypicallySecureUsers,
                features::kHttpsFirstModeV2ForEngagedSites});
        break;

      // Enable HFM, HFM with Site Engagement heuristic, HFM for typically
      // secure users, and HFM in Incognito.
      case AskBeforeHttpDialogControllerTestType::kAll:
        // HFM pref is enabled in SetUpOnMainThread.
        feature_list_.InitWithFeatures(
            /*enabled_features=*/
            {
                features::kHttpsFirstDialogUi,
                features::kHttpsFirstModeV2ForEngagedSites,
                features::kHttpsFirstModeV2ForTypicallySecureUsers,
                features::kHttpsFirstModeForAdvancedProtectionUsers,
                features::kHttpsFirstModeIncognito,
                features::kHttpsFirstBalancedMode,
                features::kHttpsFirstBalancedModeAutoEnable,
            },
            /*disabled_features=*/{});
        break;
    }

    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    // By default allow all hosts on HTTPS.
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    host_resolver()->AddRule("*", "127.0.0.1");

    // Set up "bad-https.com" as a hostname with an SSL error. HTTPS upgrades to
    // this host will fail.
    scoped_refptr<net::X509Certificate> cert(https_server_.GetCertificate());
    net::CertVerifyResult verify_result;
    verify_result.is_issued_by_known_root = false;
    verify_result.verified_cert = cert;
    verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;
    mock_cert_verifier_.mock_cert_verifier()->AddResultForCertAndHost(
        cert, "bad-https.com", verify_result,
        net::ERR_CERT_COMMON_NAME_INVALID);

    http_server_.AddDefaultHandlers(GetChromeTestDataDir());
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(http_server_.Start());
    ASSERT_TRUE(https_server_.Start());

    HttpsUpgradesInterceptor::SetHttpsPortForTesting(https_server()->port());
    HttpsUpgradesInterceptor::SetHttpPortForTesting(http_server()->port());

    // Incognito tests swap out the default Browser instance for an Incognito
    // window, and then should behave like kHttpsFirstMode type tests but
    // without enabling the full HFM pref.
    if (test_type() ==
        AskBeforeHttpDialogControllerTestType::kHttpsFirstModeIncognito) {
      UseIncognitoBrowser();
      SetPref(false);
    }

    // Only enable the HTTPS-First Mode pref when the test config calls for it.
    // Some of the HFM heuristics check that the preference wasn't set so as
    // not to override user preference (e.g. if the user changed the pref by
    // turning it off from the UI, we don't want to override it).
    if (test_type() ==
            AskBeforeHttpDialogControllerTestType::kHttpsFirstModeOnly ||
        test_type() == AskBeforeHttpDialogControllerTestType::kAll) {
      SetPref(true);
    }

    if (test_type() ==
            AskBeforeHttpDialogControllerTestType::kHttpsFirstBalancedMode ||
        test_type() == AskBeforeHttpDialogControllerTestType::kAll) {
      SetBalancedPref(true);
    }
  }

  void TearDownOnMainThread() override {
    browser()->profile()->GetPrefs()->ClearPref(prefs::kHttpsOnlyModeEnabled);
    browser()->profile()->GetPrefs()->ClearPref(
        prefs::kHttpsOnlyModeAutoEnabled);
    browser()->profile()->GetPrefs()->ClearPref(prefs::kHttpsUpgradeFallbacks);
    browser()->profile()->GetPrefs()->ClearPref(
        prefs::kHttpsUpgradeNavigations);
    browser()->profile()->GetPrefs()->ClearPref(prefs::kHttpsFirstBalancedMode);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  // Incognito testing support
  //
  // Returns the active Browser for the test type being run.
  Browser* GetBrowser() const {
    return incognito_browser_ ? incognito_browser_.get() : browser();
  }
  // Call to use an Incognito browser rather than the default.
  void UseIncognitoBrowser() {
    ASSERT_EQ(nullptr, incognito_browser_.get());
    incognito_browser_ = CreateIncognitoBrowser();
  }
  bool IsIncognito() const { return incognito_browser_ != nullptr; }

 protected:
  AskBeforeHttpDialogControllerTestType test_type() const { return GetParam(); }

  void SetPref(bool enabled) {
    auto* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(prefs::kHttpsOnlyModeEnabled, enabled);
  }

  void SetBalancedPref(bool enabled) {
    auto* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(prefs::kHttpsFirstBalancedMode, enabled);
  }

  net::EmbeddedTestServer* http_server() { return &http_server_; }
  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  // TODO(https://crbug.com/423465927): Explore a better approach to make the
  // existing tests run with the prewarm feature enabled.
  test::ScopedPrewarmFeatureList prewarm_feature_list_{
      test::ScopedPrewarmFeatureList::PrewarmState::kDisabled};
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer http_server_{net::EmbeddedTestServer::TYPE_HTTP};
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  content::ContentMockCertVerifier mock_cert_verifier_;
  base::HistogramTester histograms_;
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> incognito_browser_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    AskBeforeHttpDialogControllerUiTest,
    ::testing::Values(
        AskBeforeHttpDialogControllerTestType::kHttpsFirstModeOnly,
        AskBeforeHttpDialogControllerTestType::kHttpsFirstModeIncognito,
        AskBeforeHttpDialogControllerTestType::kHttpsFirstBalancedMode,
        AskBeforeHttpDialogControllerTestType::kAll),
    // Map param to a human-readable string for better test output.
    [](testing::TestParamInfo<AskBeforeHttpDialogControllerTestType> input_type)
        -> std::string {
      switch (input_type.param) {
        case AskBeforeHttpDialogControllerTestType::kHttpsFirstModeOnly:
          return "HttpsFirstModeOnly";
        case AskBeforeHttpDialogControllerTestType::kHttpsFirstModeIncognito:
          return "HttpsFirstModeIncognito";
        case AskBeforeHttpDialogControllerTestType::kHttpsFirstBalancedMode:
          return "HttpsFirstBalancedMode";
        case AskBeforeHttpDialogControllerTestType::kAll:
          return "AllFeatures";
      }
    });

// If the user navigates to an HTTPS URL, the navigation should end up on that
// exact URL, even if the site has an SSL error.
IN_PROC_BROWSER_TEST_P(AskBeforeHttpDialogControllerUiTest,
                       DirectHttpsNavigationFailed_NoWarningShown) {
  GURL https_url = https_server()->GetURL("bad-https.com", "/simple.html");
  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();

  content::NavigateToURLBlockUntilNavigationsComplete(contents, https_url, 1);
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());

  // The SSL error should show regardless of the HFM state.
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(contents));

  RunTestSequence(InAnyContext(
      EnsureNotPresent(AskBeforeHttpDialogController::kContinueButtonId)));
}

// If the user triggers an Ask-before-HTTP warning for a host and then
// clicks through the dialog, they should end up on the HTTP URL.
IN_PROC_BROWSER_TEST_P(AskBeforeHttpDialogControllerUiTest,
                       FailedUpgrade_WarningShown_ContinueToSite) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestTab);

  GURL http_url = http_server()->GetURL("bad-https.com", "/simple.html");
  GURL https_url = https_server()->GetURL("bad-https.com", "/simple.html");

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  content::NavigateToURLBlockUntilNavigationsComplete(contents, http_url, 1);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  RunTestSequence(
      InAnyContext(
          WaitForShow(AskBeforeHttpDialogController::kContinueButtonId)),
      InSameContext(
          InstrumentTab(kTestTab),
          PressButton(AskBeforeHttpDialogController::kContinueButtonId),
          WaitForWebContentsNavigation(kTestTab, http_url)));
}

// If the user triggers an Ask-before-HTTP warning for a host and then
// clicks the "Go back" button, they should navigate back to the previous
// page (about:blank in this case).
IN_PROC_BROWSER_TEST_P(AskBeforeHttpDialogControllerUiTest,
                       FailedUpgrade_WarningShown_GoBack) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestTab);

  GURL http_url = http_server()->GetURL("bad-https.com", "/simple.html");
  GURL https_url = https_server()->GetURL("bad-https.com", "/simple.html");

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  content::NavigateToURLBlockUntilNavigationsComplete(contents, http_url, 1);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  RunTestSequence(
      InAnyContext(WaitForShow(AskBeforeHttpDialogController::kGoBackButtonId)),
      InSameContext(
          InstrumentTab(kTestTab),
          PressButton(AskBeforeHttpDialogController::kGoBackButtonId),
          WaitForWebContentsNavigation(kTestTab, GURL("about:blank"))));

  EXPECT_EQ(GURL("about:blank"), contents->GetLastCommittedURL());
}

// If the user triggers an Ask-before-HTTP warning for a host and then
// clicks the "learn more" link, a new tab should open to the help
// center URL.
IN_PROC_BROWSER_TEST_P(AskBeforeHttpDialogControllerUiTest,
                       FailedUpgrade_WarningShown_ClickLearnMore) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestTab);

  GURL http_url = http_server()->GetURL("bad-https.com", "/simple.html");
  GURL https_url = https_server()->GetURL("bad-https.com", "/simple.html");

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  content::NavigateToURLBlockUntilNavigationsComplete(contents, http_url, 1);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  RunTestSequence(
      InAnyContext(
          WaitForShow(AskBeforeHttpDialogController::kDescriptionTextId)),
      InSameContext(
          InstrumentTab(kTestTab),
          WithView(AskBeforeHttpDialogController::kDescriptionTextId,
                   [](views::StyledLabel* description_label) {
                     description_label->ClickFirstLinkForTesting();
                   }),
          WaitForHide(AskBeforeHttpDialogController::kDescriptionTextId)));

  // New tab should include the p-link "first_mode".
  EXPECT_EQ(GetBrowser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetVisibleURL()
                .query(),
            "p=first_mode");
}

// If the user triggers an Ask-before-HTTP warning for a host, then clicking
// the browser back button should dismiss the dialog, but if the user then
// clicks the forward button they should end up back on the HTTP page and
// the warning dialog should be showing.
IN_PROC_BROWSER_TEST_P(AskBeforeHttpDialogControllerUiTest,
                       FailedUpgrade_WarningShown_BackForwardNavigations) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestTab);

  GURL http_url = http_server()->GetURL("bad-https.com", "/simple.html");
  GURL https_url = https_server()->GetURL("bad-https.com", "/simple.html");

  auto* contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  content::NavigateToURLBlockUntilNavigationsComplete(contents, http_url, 1);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  RunTestSequence(
      InAnyContext(
          WaitForShow(AskBeforeHttpDialogController::kContinueButtonId)),
      InSameContext(
          InstrumentTab(kTestTab),
          // Press the browser back button. Dialog should be dismissed.
          PressButton(kToolbarBackButtonElementId),
          WaitForWebContentsNavigation(kTestTab, GURL("about:blank")),
          EnsureNotPresent(AskBeforeHttpDialogController::kContinueButtonId),
          // Press the browser forward button. Dialog should be shown again.
          PressButton(kToolbarForwardButtonElementId),
          WaitForWebContentsNavigation(kTestTab, http_url),
          EnsurePresent(AskBeforeHttpDialogController::kContinueButtonId)));
}
