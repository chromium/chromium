// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/media/kaleidoscope/constants.h"
#include "chrome/browser/media/kaleidoscope/kaleidoscope_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace {

static constexpr char const kTestPagePath[] = "/media/unified_autoplay.html";

}  // anonymous namespace

class KaleidoscopeTabHelperBrowserTest : public InProcessBrowserTest {
 public:
  KaleidoscopeTabHelperBrowserTest() = default;

  ~KaleidoscopeTabHelperBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    InProcessBrowserTest::SetUpOnMainThread();
  }

  bool AttemptPlay(content::WebContents* web_contents) {
    bool played = false;
    EXPECT_TRUE(content::ExecuteScriptWithoutUserGestureAndExtractBool(
        web_contents, "attemptPlay();", &played));
    return played;
  }

  bool NavigateInRenderer(content::WebContents* web_contents, const GURL& url) {
    content::TestNavigationObserver observer(web_contents);

    bool result = content::ExecuteScriptWithoutUserGesture(
        web_contents, "window.location = '" + url.spec() + "';");

    if (result)
      observer.Wait();
    return result;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  KaleidoscopeTabHelper* GetTabHelper() {
    return KaleidoscopeTabHelper::FromWebContents(GetWebContents());
  }

  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(KaleidoscopeTabHelperBrowserTest,
                       NotOpenedFromKaleidoscope) {
  const GURL kTestPageUrl = embedded_test_server()->GetURL(kTestPagePath);
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  NavigateParams params(browser(), kTestPageUrl, ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  // Autoplay should not be allowed since that is the default.
  EXPECT_FALSE(AttemptPlay(GetWebContents()));
  EXPECT_FALSE(GetTabHelper()->IsKaleidoscopeDerived());

  histogram_tester_.ExpectTotalCount(
      KaleidoscopeTabHelper::kKaleidoscopeNavigationHistogramName, 0);

  auto ukm_entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Media_Kaleidoscope_Navigation::kEntryName);
  ASSERT_EQ(0u, ukm_entries.size());
}

IN_PROC_BROWSER_TEST_F(KaleidoscopeTabHelperBrowserTest,
                       OpenedFromKaleidoscope) {
  const GURL kTestPageUrl = embedded_test_server()->GetURL(kTestPagePath);
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  NavigateParams params(browser(), kTestPageUrl, ui::PAGE_TRANSITION_LINK);
  params.initiator_origin =
      url::Origin::Create(GURL(kKaleidoscopeUntrustedContentUIURL));
  ui_test_utils::NavigateToURL(&params);

  // Autoplay should be allowed because this page was opened from Kaleidoscope.
  EXPECT_TRUE(AttemptPlay(GetWebContents()));
  EXPECT_TRUE(GetTabHelper()->IsKaleidoscopeDerived());

  // Autoplay should not be allowed since this is a derived navigation.
  NavigateInRenderer(GetWebContents(), kTestPageUrl);
  EXPECT_FALSE(AttemptPlay(GetWebContents()));
  EXPECT_TRUE(GetTabHelper()->IsKaleidoscopeDerived());

  histogram_tester_.ExpectBucketCount(
      KaleidoscopeTabHelper::kKaleidoscopeNavigationHistogramName,
      KaleidoscopeTabHelper::KaleidoscopeNavigation::kNormal, 1);

  auto ukm_entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Media_Kaleidoscope_Navigation::kEntryName);
  ASSERT_EQ(1u, ukm_entries.size());

  auto* ukm_entry = ukm_entries.back();
  test_ukm_recorder.ExpectEntrySourceHasUrl(ukm_entry, kTestPageUrl);

  NavigateInRenderer(GetWebContents(), embedded_test_server()->GetURL(
                                           "example.com", kTestPagePath));
  EXPECT_FALSE(GetTabHelper()->IsKaleidoscopeDerived());
}

IN_PROC_BROWSER_TEST_F(KaleidoscopeTabHelperBrowserTest,
                       SessionMetric_OpenedRecommendation) {
  const GURL kTestPageUrl = embedded_test_server()->GetURL(kTestPagePath);

  {
    // Navigate to Kaleidoscope.
    NavigateParams params(browser(), GURL(kKaleidoscopeUIURL),
                          ui::PAGE_TRANSITION_LINK);
    ui_test_utils::NavigateToURL(&params);
  }

  // Simulate a playback.
  KaleidoscopeTabHelper::FromWebContents(GetWebContents())->MarkAsSuccessful();

  {
    // Navigate away from Kaleidoscope.
    NavigateParams params(browser(), kTestPageUrl, ui::PAGE_TRANSITION_LINK);
    ui_test_utils::NavigateToURL(&params);
  }

  histogram_tester_.ExpectBucketCount(
      KaleidoscopeTabHelper::
          kKaleidoscopeOpenedMediaRecommendationHistogramName,
      true, 1);
}

IN_PROC_BROWSER_TEST_F(KaleidoscopeTabHelperBrowserTest,
                       SessionMetric_DidNotOpenRecommendation) {
  const GURL kTestPageUrl = embedded_test_server()->GetURL(kTestPagePath);

  {
    // Navigate to Kaleidoscope.
    NavigateParams params(browser(), GURL(kKaleidoscopeUIURL),
                          ui::PAGE_TRANSITION_LINK);
    ui_test_utils::NavigateToURL(&params);
  }

  {
    // Navigate away from Kaleidoscope.
    NavigateParams params(browser(), kTestPageUrl, ui::PAGE_TRANSITION_LINK);
    ui_test_utils::NavigateToURL(&params);
  }

  histogram_tester_.ExpectBucketCount(
      KaleidoscopeTabHelper::
          kKaleidoscopeOpenedMediaRecommendationHistogramName,
      false, 1);
}
