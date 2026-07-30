// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/android/suspicious_site_controller_android.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/db/fake_database_manager.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace safe_browsing {

class SuspiciousSiteBrowserTest : public AndroidBrowserTest {
 public:
  SuspiciousSiteBrowserTest() {
    feature_list_.InitAndEnableFeature(kSuspiciousSiteWarnings);
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    AndroidBrowserTest::CreatedBrowserMainParts(browser_main_parts);
    factory_.SetTestUIManager(new TestSafeBrowsingUIManager());
    factory_.SetTestDatabaseManager(new FakeSafeBrowsingDatabaseManager(
        content::GetUIThreadTaskRunner({})));
    SafeBrowsingService::RegisterFactory(&factory_);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    safe_browsing::SetSafeBrowsingState(
        Profile::FromBrowserContext(GetActiveWebContents()->GetBrowserContext())
            ->GetPrefs(),
        safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
    PlatformBrowserTest::SetUpOnMainThread();
  }

  void TearDown() override {
    SuspiciousSiteControllerAndroid::SetDialogShownCallbackForTesting(
        base::NullCallback());
    SuspiciousSiteControllerAndroid::SetDialogDismissedCallbackForTesting(
        base::NullCallback());
    PlatformBrowserTest::TearDown();
    SafeBrowsingService::RegisterFactory(nullptr);
  }

  void SetURLThreatType(const GURL& url, SBThreatType threat_type) {
    TestSafeBrowsingService* service = factory_.test_safe_browsing_service();
    ASSERT_TRUE(service);

    static_cast<FakeSafeBrowsingDatabaseManager*>(
        service->database_manager().get())
        ->AddDangerousUrl(url, threat_type);
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  TestSafeBrowsingServiceFactory* factory() { return &factory_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  TestSafeBrowsingServiceFactory factory_;
};

IN_PROC_BROWSER_TEST_F(SuspiciousSiteBrowserTest, ShowsWarningBeforeCommit) {
  GURL malicious_url = embedded_test_server()->GetURL("/title1.html");
  SetURLThreatType(malicious_url,
                   SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE);

  // Navigate to an empty page first to flush initial loads.
  content::NavigateToURLBlockUntilNavigationsComplete(
      GetActiveWebContents(), GURL("about:blank"),
      /* number_of_navigations= */ 1);

  bool dialog_shown = false;
  SuspiciousSiteControllerAndroid::SetDialogShownCallbackForTesting(
      base::BindLambdaForTesting([&]() { dialog_shown = true; }));

  content::NavigateToURLBlockUntilNavigationsComplete(
      GetActiveWebContents(), malicious_url,
      /* number_of_navigations= */ 1);

  EXPECT_TRUE(dialog_shown);
  EXPECT_TRUE(safe_browsing::SuspiciousSiteControllerAndroid::FromWebContents(
      GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_F(SuspiciousSiteBrowserTest, DismissalNavigateBack) {
  GURL malicious_url = embedded_test_server()->GetURL("/title1.html");
  SetURLThreatType(malicious_url,
                   SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE);

  bool dialog_shown = false;
  SuspiciousSiteControllerAndroid::SetDialogShownCallbackForTesting(
      base::BindLambdaForTesting([&]() { dialog_shown = true; }));

  content::NavigateToURLBlockUntilNavigationsComplete(
      GetActiveWebContents(), malicious_url,
      /* number_of_navigations= */ 1);

  EXPECT_TRUE(dialog_shown);
  auto* controller =
      safe_browsing::SuspiciousSiteControllerAndroid::FromWebContents(
          GetActiveWebContents());
  ASSERT_TRUE(controller);

  content::TestNavigationObserver observer(GetActiveWebContents(), 1);
  controller->OnGoBackButtonClicked();
  observer.Wait();
}

IN_PROC_BROWSER_TEST_F(SuspiciousSiteBrowserTest, DismissalContinueAnyway) {
  GURL malicious_url = embedded_test_server()->GetURL("/title1.html");
  SetURLThreatType(malicious_url,
                   SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE);

  bool dialog_shown = false;
  SuspiciousSiteControllerAndroid::SetDialogShownCallbackForTesting(
      base::BindLambdaForTesting([&]() { dialog_shown = true; }));

  content::NavigateToURLBlockUntilNavigationsComplete(
      GetActiveWebContents(), malicious_url,
      /* number_of_navigations= */ 1);

  EXPECT_TRUE(dialog_shown);
  auto* controller =
      safe_browsing::SuspiciousSiteControllerAndroid::FromWebContents(
          GetActiveWebContents());
  ASSERT_TRUE(controller);

  controller->OnContinueButtonClicked();

  EXPECT_FALSE(safe_browsing::SuspiciousSiteControllerAndroid::FromWebContents(
      GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_F(SuspiciousSiteBrowserTest,
                       AllowlistedSiteBypassesWarning) {
  GURL malicious_url = embedded_test_server()->GetURL("/title1.html");
  SetURLThreatType(malicious_url,
                   SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE);

  // 1. Navigate and click Continue anyway to allowlist the site.
  content::NavigateToURLBlockUntilNavigationsComplete(
      GetActiveWebContents(), malicious_url,
      /* number_of_navigations= */ 1);

  auto* controller =
      safe_browsing::SuspiciousSiteControllerAndroid::FromWebContents(
          GetActiveWebContents());
  ASSERT_TRUE(controller);
  controller->OnContinueButtonClicked();
  EXPECT_FALSE(safe_browsing::SuspiciousSiteControllerAndroid::FromWebContents(
      GetActiveWebContents()));

  // 2. Navigate away to reset state.
  content::NavigateToURLBlockUntilNavigationsComplete(
      GetActiveWebContents(), GURL("about:blank"),
      /* number_of_navigations= */ 1);

  // 3. Navigate back to the malicious URL. Because it was allowlisted, no
  // dialog should be shown.
  bool dialog_shown = false;
  SuspiciousSiteControllerAndroid::SetDialogShownCallbackForTesting(
      base::BindLambdaForTesting([&]() { dialog_shown = true; }));
  content::NavigateToURLBlockUntilNavigationsComplete(
      GetActiveWebContents(), malicious_url,
      /* number_of_navigations= */ 1);

  EXPECT_FALSE(dialog_shown);
  EXPECT_FALSE(safe_browsing::SuspiciousSiteControllerAndroid::FromWebContents(
      GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_F(SuspiciousSiteBrowserTest,
                       NavigateAwayDismissesWarning) {
  GURL malicious_url = embedded_test_server()->GetURL("/title1.html");
  GURL safe_url = embedded_test_server()->GetURL("/title2.html");
  SetURLThreatType(malicious_url,
                   SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE);

  bool dialog_shown = false;
  SuspiciousSiteControllerAndroid::SetDialogShownCallbackForTesting(
      base::BindLambdaForTesting([&]() { dialog_shown = true; }));

  content::NavigateToURLBlockUntilNavigationsComplete(
      GetActiveWebContents(), malicious_url,
      /* number_of_navigations= */ 1);

  EXPECT_TRUE(dialog_shown);
  EXPECT_TRUE(safe_browsing::SuspiciousSiteControllerAndroid::FromWebContents(
      GetActiveWebContents()));

  // Navigate to a distinct safe URL without dismissing the dialog.
  content::NavigateToURLBlockUntilNavigationsComplete(
      GetActiveWebContents(), safe_url,
      /* number_of_navigations= */ 1);

  EXPECT_FALSE(safe_browsing::SuspiciousSiteControllerAndroid::FromWebContents(
      GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_F(SuspiciousSiteBrowserTest,
                       DISABLED_TabSwitchingHidesAndRestoresWarning) {
  GURL malicious_url = embedded_test_server()->GetURL("/title1.html");
  SetURLThreatType(malicious_url,
                   SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE);

  bool dialog_shown = false;
  SuspiciousSiteControllerAndroid::SetDialogShownCallbackForTesting(
      base::BindLambdaForTesting([&]() { dialog_shown = true; }));

  content::NavigateToURLBlockUntilNavigationsComplete(
      GetActiveWebContents(), malicious_url,
      /* number_of_navigations= */ 1);

  EXPECT_TRUE(dialog_shown);
  auto* controller =
      safe_browsing::SuspiciousSiteControllerAndroid::FromWebContents(
          GetActiveWebContents());
  ASSERT_TRUE(controller);

  // Hide tab (simulating switching away to another tab).
  GetActiveWebContents()->WasHidden();

  // Show tab again (simulating switching back to this tab).
  GetActiveWebContents()->WasShown();

  EXPECT_TRUE(safe_browsing::SuspiciousSiteControllerAndroid::FromWebContents(
      GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_F(SuspiciousSiteBrowserTest,
                       InterstitialPrecedenceOverWarning) {
  GURL phishing_url = embedded_test_server()->GetURL("/title1.html");
  GURL redirect_url =
      embedded_test_server()->GetURL("/server-redirect?" + phishing_url.spec());

  // Mark 1st URL in redirect chain as suspicious warning, and 2nd URL as
  // phishing interstitial.
  SetURLThreatType(redirect_url,
                   SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE);
  SetURLThreatType(phishing_url, SBThreatType::SB_THREAT_TYPE_URL_PHISHING);

  bool dialog_shown = false;
  SuspiciousSiteControllerAndroid::SetDialogShownCallbackForTesting(
      base::BindLambdaForTesting([&]() { dialog_shown = true; }));

  content::NavigateToURLBlockUntilNavigationsComplete(
      GetActiveWebContents(), redirect_url,
      /* number_of_navigations= */ 1);

  // Full page interstitial takes precedence; warning dialog should not be
  // shown.
  EXPECT_FALSE(dialog_shown);
  EXPECT_FALSE(safe_browsing::SuspiciousSiteControllerAndroid::FromWebContents(
      GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_F(SuspiciousSiteBrowserTest,
                       WarningOnIntermediateRedirect) {
  GURL target_url = embedded_test_server()->GetURL("/title1.html");
  GURL redirect_url =
      embedded_test_server()->GetURL("/server-redirect?" + target_url.spec());

  // Mark intermediate redirect URL as suspicious, but final destination as
  // safe.
  SetURLThreatType(redirect_url,
                   SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE);

  bool dialog_shown = false;
  SuspiciousSiteControllerAndroid::SetDialogShownCallbackForTesting(
      base::BindLambdaForTesting([&]() { dialog_shown = true; }));

  content::NavigateToURLBlockUntilNavigationsComplete(
      GetActiveWebContents(), redirect_url,
      /* number_of_navigations= */ 1);

  EXPECT_TRUE(dialog_shown);
  EXPECT_TRUE(safe_browsing::SuspiciousSiteControllerAndroid::FromWebContents(
      GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_F(SuspiciousSiteBrowserTest, ErrorPageDismissesWarning) {
  GURL error_url("https://127.0.0.1:1/");
  SetURLThreatType(error_url,
                   SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE);

  content::NavigateToURLBlockUntilNavigationsComplete(
      GetActiveWebContents(), error_url,
      /* number_of_navigations= */ 1);

  // Error page should dismiss warning controller.
  EXPECT_FALSE(safe_browsing::SuspiciousSiteControllerAndroid::FromWebContents(
      GetActiveWebContents()));
}

}  // namespace safe_browsing
