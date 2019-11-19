// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/trigger_creator.h"
#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/triggers/ad_redirect_trigger.h"
#include "components/safe_browsing/triggers/mock_trigger_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace safe_browsing {

class AdRedirectTriggerBrowserTest : public InProcessBrowserTest,
                                     public UrlListManager::Observer {
 public:
  AdRedirectTriggerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(kAdRedirectTriggerFeature);
  }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    current_browser_ = InProcessBrowserTest::browser();
    FramebustBlockTabHelper::FromWebContents(GetWebContents())
        ->manager()
        ->AddObserver(this);
  }

  // UrlListManager::Observer:
  void BlockedUrlAdded(int32_t id, const GURL& blocked_url) override {
    if (!blocked_url_added_closure_.is_null())
      std::move(blocked_url_added_closure_).Run();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  FramebustBlockTabHelper* GetFramebustTabHelper() {
    return FramebustBlockTabHelper::FromWebContents(GetWebContents());
  }

  Browser* browser() { return current_browser_; }

  void CreateAndSetBrowser() {
    current_browser_ = CreateBrowser(browser()->profile());
  }

  bool NavigateIframeToUrlWithoutGesture(content::WebContents* contents,
                                         const std::string iframe_id,
                                         const GURL& url) {
    const char kScript[] = R"(
        var iframe = document.getElementById('%s');
        iframe.src='%s'
    )";
    content::TestNavigationObserver load_observer(contents);
    bool result = content::ExecuteScriptWithoutUserGesture(
        contents,
        base::StringPrintf(kScript, iframe_id.c_str(), url.spec().c_str()));
    load_observer.Wait();
    return result;
  }

  void CreateTrigger() {
    Profile* profile =
        Profile::FromBrowserContext(GetWebContents()->GetBrowserContext());

    profile->GetPrefs()->SetBoolean(
        prefs::kSafeBrowsingExtendedReportingOptInAllowed, true);
    profile->GetPrefs()->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                                    true);

    safe_browsing::TriggerCreator::MaybeCreateTriggersForWebContents(
        profile, GetWebContents());
    safe_browsing::AdRedirectTrigger* ad_redirect_trigger =
        safe_browsing::AdRedirectTrigger::FromWebContents(GetWebContents());
    ad_redirect_trigger->SetDelayForTest(0, 0);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::OnceClosure blocked_url_added_closure_;
  Browser* current_browser_;
};

// Check that a report is sent when the source of a blocked redirect is an ad.
// TODO(https://crbug.com/978405) Disabled due to flakiness.
IN_PROC_BROWSER_TEST_F(AdRedirectTriggerBrowserTest,
                       DISABLED_BlockRedirectNavigation_FromAd) {
  base::HistogramTester histogram_tester;
  CreateTrigger();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/iframe.html"));

  // Sets the Iframe that will cause the blocked redirect to a google ad.
  GURL child_url =
      embedded_test_server()->GetURL("tpc.googlesyndication.com", "/safeframe");
  NavigateIframeToUrlWithoutGesture(GetWebContents(), "test", child_url);

  content::RenderFrameHost* child =
      content::ChildFrameAt(GetWebContents()->GetMainFrame(), 0);
  EXPECT_EQ(child_url, child->GetLastCommittedURL());

  GURL redirect_url = embedded_test_server()->GetURL("b.com", "/title1.html");

  base::RunLoop block_waiter;
  blocked_url_added_closure_ = block_waiter.QuitWhenIdleClosure();
  child->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16(base::StringPrintf("window.top.location = '%s';",
                                            redirect_url.spec().c_str())),
      base::NullCallback());
  // Navigate away - this will trigger logging of the UMA.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));

  block_waiter.RunUntilIdle();

  histogram_tester.ExpectBucketCount(kAdRedirectTriggerActionMetricName,
                                     AdRedirectTriggerAction::REDIRECT_CHECK,
                                     1);
  histogram_tester.ExpectBucketCount(kAdRedirectTriggerActionMetricName,
                                     AdRedirectTriggerAction::AD_REDIRECT, 1);
}

// Blocked redirect navigation will not trigger a report if the source of the
// redirect is not an ad frame, even if an ad frame exists on the same page.
IN_PROC_BROWSER_TEST_F(AdRedirectTriggerBrowserTest,
                       BlockRedirectNavigation_NotFromAdFrameOnPageWithAd) {
  base::HistogramTester histogram_tester;
  CreateTrigger();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/iframe.html"));

  // Create an ad subframe.
  GetWebContents()->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("f = document.createElement('google_ads_iframe');"
                         "f.srcdoc = '<script>var x = 1</script>';"
                         "document.body.appendChild(f);"),
      base::NullCallback());

  // Cause blocked redirect
  GURL child_url = embedded_test_server()->GetURL("a.com", "/title1.html");
  NavigateIframeToUrlWithoutGesture(GetWebContents(), "test", child_url);

  content::RenderFrameHost* child =
      content::ChildFrameAt(GetWebContents()->GetMainFrame(), 0);
  EXPECT_EQ(child_url, child->GetLastCommittedURL());

  GURL redirect_url = embedded_test_server()->GetURL("b.com", "/title1.html");

  base::RunLoop block_waiter;
  blocked_url_added_closure_ = block_waiter.QuitClosure();
  child->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16(base::StringPrintf("window.top.location = '%s';",
                                            redirect_url.spec().c_str())),
      base::NullCallback());
  block_waiter.Run();

  EXPECT_TRUE(
      base::Contains(GetFramebustTabHelper()->blocked_urls(), redirect_url));

  // Navigate away - this will trigger logging of the UMA.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  histogram_tester.ExpectBucketCount(kAdRedirectTriggerActionMetricName,
                                     AdRedirectTriggerAction::REDIRECT_CHECK,
                                     1);
  histogram_tester.ExpectBucketCount(
      kAdRedirectTriggerActionMetricName,
      AdRedirectTriggerAction::REDIRECT_NO_GOOGLE_AD, 1);
}

// Blocked redirect navigation will not trigger a report if the source of the
// redirect is not an ad frame.
IN_PROC_BROWSER_TEST_F(AdRedirectTriggerBrowserTest,
                       BlockRedirectNavigation_NoAdsOnPage) {
  base::HistogramTester histogram_tester;
  CreateTrigger();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/iframe.html"));

  // Cause blocked redirect
  GURL child_url = embedded_test_server()->GetURL("a.com", "/title1.html");
  NavigateIframeToUrlWithoutGesture(GetWebContents(), "test", child_url);

  content::RenderFrameHost* child =
      content::ChildFrameAt(GetWebContents()->GetMainFrame(), 0);
  EXPECT_EQ(child_url, child->GetLastCommittedURL());

  GURL redirect_url = embedded_test_server()->GetURL("b.com", "/title1.html");

  base::RunLoop block_waiter;
  blocked_url_added_closure_ = block_waiter.QuitClosure();
  child->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16(base::StringPrintf("window.top.location = '%s';",
                                            redirect_url.spec().c_str())),
      base::NullCallback());
  block_waiter.Run();

  EXPECT_TRUE(
      base::Contains(GetFramebustTabHelper()->blocked_urls(), redirect_url));

  // Navigate away - this will trigger logging of the UMA.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  histogram_tester.ExpectBucketCount(kAdRedirectTriggerActionMetricName,
                                     AdRedirectTriggerAction::REDIRECT_CHECK,
                                     1);
  histogram_tester.ExpectBucketCount(
      kAdRedirectTriggerActionMetricName,
      AdRedirectTriggerAction::REDIRECT_NO_GOOGLE_AD, 1);
}

}  // namespace safe_browsing
