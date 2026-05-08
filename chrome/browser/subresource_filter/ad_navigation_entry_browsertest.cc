// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/subresource_filter/content/browser/ad_tagging_browser_test_utils.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/url_constants.h"

namespace subresource_filter {

// Tests Back-To-Ad Intervention that allows the browser to skip over ad-related
// entries that were silently inserted into session history when navigating via
// back/forward buttons.
//
// Note: This test suite lives in the chrome/ layer because the ad classifying
// functionality depends on Chrome-specific logic.
//
// TODO(yaoxia): Rename to `BackToAdInterventionBrowserTest`.
class AdNavigationEntryBrowserTest : public SubresourceFilterBrowserTest {
 public:
  AdNavigationEntryBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kBackToAdIntervention);
  }

  ~AdNavigationEntryBrowserTest() override = default;

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();

    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

    host_resolver()->AddRule("*", "127.0.0.1");

    SetRulesetWithRules({testing::CreateSuffixRule("ad_script.js"),
                         testing::CreateSuffixRule("ad=true")});

    // Navigate to the test page that includes ad_script.js.
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html")));

    // History now contains: [new tab page, frame_factory.html*].
    ASSERT_EQ(GetWebContents()->GetController().GetEntryCount(), 2u);
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  GURL GetURL(const std::string& page) {
    return embedded_test_server()->GetURL("/ad_tagging/" + page);
  }

  GURL GetURL(const std::string& hostname, const std::string& page) {
    return embedded_test_server()->GetURL(hostname, "/ad_tagging/" + page);
  }

  // Returns a vector of the URLs associated with each entry in the current
  // navigation history.
  std::vector<GURL> GetNavigationUrls() {
    std::vector<GURL> results;
    for (int i = 0; i < GetWebContents()->GetController().GetEntryCount();
         ++i) {
      results.push_back(
          GetWebContents()->GetController().GetEntryAtIndex(i)->GetURL());
    }

    return results;
  }

  void GoBack() {
    content::TestNavigationObserver observer(GetWebContents());
    GetWebContents()->GetController().GoBack();
    observer.Wait();
  }

  void GoForward() {
    content::TestNavigationObserver observer(GetWebContents());
    GetWebContents()->GetController().GoForward();
    observer.Wait();
  }

  ukm::TestAutoSetUkmRecorder* ukm_recorder() { return ukm_recorder_.get(); }

  bool RecordedUkmUseCounter(const blink::mojom::WebFeature& expected_entry,
                             const GURL& expected_url) {
    const auto& entries = ukm_recorder()->GetEntriesByName(
        ukm::builders::Blink_UseCounter::kEntryName);
    for (const ukm::mojom::UkmEntry* entry : entries) {
      const ukm::UkmSource* src =
          ukm_recorder()->GetSourceForSourceId(entry->source_id);

      if (src->url() != expected_url) {
        continue;
      }

      const int64_t* metric = ukm_recorder()->GetEntryMetric(
          entry, ukm::builders::Blink_UseCounter::kFeatureName);
      if (*metric == static_cast<int>(expected_entry)) {
        return true;
      }
    }

    return false;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

IN_PROC_BROWSER_TEST_F(AdNavigationEntryBrowserTest,
                       AdReplaceStateFollowedByNonAdPushState) {
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('replaced_state');
    history.pushState({}, '', 'new_state')
  )"));

  EXPECT_THAT(
      GetNavigationUrls(),
      ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                             GetURL("replaced_state"), GetURL("new_state")));

  // Back from 'new_state'. The previous entry `replaced_state` is not skipped,
  // because it is not an ad entry creator.
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GetURL("replaced_state"));
}

IN_PROC_BROWSER_TEST_F(AdNavigationEntryBrowserTest,
                       NonAdReplaceStateFollowedByAdPushState) {
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    history.replaceState({}, '', 'replaced_state');
    executeHistoryPushStateFromAdScript('new_state');
  )"));

  EXPECT_THAT(
      GetNavigationUrls(),
      ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                             GetURL("replaced_state"), GetURL("new_state")));

  // Back from 'new_state'. The previous entry `replaced_state` is not skipped,
  // because it is not created by ad.
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GetURL("replaced_state"));
}

IN_PROC_BROWSER_TEST_F(AdNavigationEntryBrowserTest,
                       AdReplaceStateFollowedByAdPushState) {
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('replaced_state');
    executeHistoryPushStateFromAdScript('new_state');
  )"));

  EXPECT_THAT(
      GetNavigationUrls(),
      ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                             GetURL("replaced_state"), GetURL("new_state")));

  // Back from 'new_state'. The previous entry `replaced_state` is skipped,
  // because it is both created by ad and an ad entry creator.
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GURL(url::kAboutBlankURL));

  // Forward from `kAboutBlankURL` should also skip `replaced_state`.
  GoForward();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GetURL("new_state"));

  // Close browser to trigger metric recording.
  CloseBrowserSynchronously(browser());

  // UKM metrics are attributed to the initial URL (`frame_factory.html`) of
  // page where the back navigation started.
  ASSERT_TRUE(
      RecordedUkmUseCounter(blink::mojom::WebFeature::kHistoryGoBackWouldSkipAd,
                            GetURL("frame_factory.html")));

  // UKM metrics are not logged for `about:blank` pages. Therefore, the
  // `kHistoryGoForwardWouldSkipAd` metric is NOT expected to be recorded here.
  ASSERT_FALSE(RecordedUkmUseCounter(
      blink::mojom::WebFeature::kHistoryGoForwardWouldSkipAd,
      GURL(url::kAboutBlankURL)));
}

IN_PROC_BROWSER_TEST_F(
    AdNavigationEntryBrowserTest,
    CrossOriginNavigationFollowedByAdReplaceStateFollowedByAdPushState) {
  // Navigate to a cross-origin page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetURL("b.com", "frame_factory.html")));

  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('replaced_state');
    executeHistoryPushStateFromAdScript('new_state');
  )"));

  EXPECT_THAT(GetNavigationUrls(),
              ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                                     GetURL("frame_factory.html"),
                                     GetURL("b.com", "replaced_state"),
                                     GetURL("b.com", "new_state")));

  // Back from 'new_state'. The previous entry `replaced_state` is skipped,
  // because it is both created by ad and an ad entry creator.
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(),
            GetURL("frame_factory.html"));

  // Forward from `frame_factory.html` should also skip `replaced_state`.
  GoForward();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(),
            GetURL("b.com", "new_state"));

  // Close browser to trigger metric recording.
  CloseBrowserSynchronously(browser());

  // UKM metrics are attributed to the initial URL (`frame_factory.html` under
  // `b.com`) of page where the back navigation started.
  ASSERT_TRUE(
      RecordedUkmUseCounter(blink::mojom::WebFeature::kHistoryGoBackWouldSkipAd,
                            GetURL("b.com", "frame_factory.html")));

  // UKM metrics are attributed to the initial URL (`frame_factory.html` under
  // `a.com`) of page where the forward navigation started.
  ASSERT_TRUE(RecordedUkmUseCounter(
      blink::mojom::WebFeature::kHistoryGoForwardWouldSkipAd,
      GetURL("frame_factory.html")));
}

IN_PROC_BROWSER_TEST_F(AdNavigationEntryBrowserTest,
                       AdReplaceStateFollowedByAdPushState_NewTab) {
  // Open a new foreground tab. This gives us a pristine history stack that
  // starts at frame_factory.html.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetURL("frame_factory.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Quick sanity check to ensure our new active tab only has 1 history entry.
  ASSERT_EQ(GetWebContents()->GetController().GetEntryCount(), 1);

  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('replaced_state');
    executeHistoryPushStateFromAdScript('new_state');
  )"));

  EXPECT_THAT(
      GetNavigationUrls(),
      ::testing::ElementsAre(GetURL("replaced_state"), GetURL("new_state")));

  // The only previous entry in the history stack (`replaced_state`) was created
  // is flagged as skippable. Because there are no valid entries to skip back
  // to, it prevents backward navigation entirely.
  EXPECT_FALSE(GetWebContents()->GetController().CanGoBack());
}

IN_PROC_BROWSER_TEST_F(AdNavigationEntryBrowserTest,
                       AdReplaceStateFollowedByAdReplaceState) {
  // Invoking history.replaceState twice from an ad script should not flag the
  // entry as a candidate for skipping, as it lacks an ad-entry-creator signal.
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('replaced_state1');
    executeHistoryReplaceStateFromAdScript('replaced_state2');
  )"));

  EXPECT_THAT(GetNavigationUrls(),
              ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                                     GetURL("replaced_state2")));

  // Back from 'replaced_state2'. There are no intermediate entries to skip.
  // Expect normal back navigation to kAboutBlankURL.
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GURL(url::kAboutBlankURL));
}

IN_PROC_BROWSER_TEST_F(AdNavigationEntryBrowserTest,
                       AdReplaceStateFollowedByAdFragmentNavigation) {
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('replaced_state');
    executeLocationAssignFromAdScript("#foo");
  )"));

  EXPECT_THAT(GetNavigationUrls(),
              ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                                     GetURL("replaced_state"),
                                     GetURL("replaced_state#foo")));

  // Back from 'replaced_state#foo'. The previous entry `replaced_state` is
  // skipped, because it is both created by ad and an ad entry creator.
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GURL(url::kAboutBlankURL));
}

IN_PROC_BROWSER_TEST_F(AdNavigationEntryBrowserTest,
                       AdReplaceStateFollowedByAdLocationAssign) {
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('replaced_state');
  )"));

  GURL new_url = GetURL("test_div.html");
  content::TestNavigationManager navigation_manager(GetWebContents(), new_url);
  ASSERT_TRUE(content::ExecJs(GetWebContents(), content::JsReplace(R"(
    executeLocationAssignFromAdScript($1);
  )",
                                                                   new_url)));
  EXPECT_TRUE(navigation_manager.WaitForNavigationFinished());

  EXPECT_THAT(GetNavigationUrls(),
              ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                                     GetURL("replaced_state"),
                                     GetURL("test_div.html")));

  // A cross-document navigation triggered by an ad script should not flag
  // the originating history entry (`replaced_state`).
  //
  // Back from 'test_div.html'. The previous entry `replaced_state` is not
  // skipped.
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GetURL("replaced_state"));
}

IN_PROC_BROWSER_TEST_F(AdNavigationEntryBrowserTest,
                       AdLocationReplaceFollowedByAdPushState) {
  GURL new_url = GetURL("frame_factory.html");
  content::TestNavigationManager navigation_manager(GetWebContents(), new_url);
  ASSERT_TRUE(content::ExecJs(GetWebContents(), content::JsReplace(R"(
    executeLocationReplaceFromAdScript($1);
  )",
                                                                   new_url)));
  EXPECT_TRUE(navigation_manager.WaitForNavigationFinished());

  // Wait until the ad script has loaded and defined our function.
  const std::string wait_for_function_script = R"(
    new Promise(resolve => {
      const intervalId = setInterval(() => {
        if (typeof executeHistoryPushStateFromAdScript === 'function') {
          clearInterval(intervalId);
          resolve(true);
        }
      }, 10); // Poll every 10ms.
    })
  )";
  ASSERT_TRUE(content::EvalJs(GetWebContents(), wait_for_function_script)
                  .ExtractBool());

  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryPushStateFromAdScript('new_state');
  )"));

  EXPECT_THAT(GetNavigationUrls(),
              ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                                     GetURL("frame_factory.html"),
                                     GetURL("new_state")));

  // A cross-document replacement navigation triggered by an ad script should
  // not flag the destination history entry (`frame_factory.html`).
  //
  // Back from 'new_state'. The previous entry `frame_factory.html` is not
  // skipped.
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(),
            GetURL("frame_factory.html"));
}

IN_PROC_BROWSER_TEST_F(AdNavigationEntryBrowserTest,
                       AdReplaceStateFollowedByAdFramePushState) {
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('replaced_state');
  )"));

  // Create an ad subframe.
  GURL ad_url = GetURL("test_div.html?ad=true");
  content::RenderFrameHost* ad_frame = CreateSrcFrame(GetWebContents(), ad_url);

  ASSERT_TRUE(content::ExecJs(ad_frame, R"(
    history.pushState({}, '', 'new_state')
  )"));

  EXPECT_THAT(GetNavigationUrls(),
              ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                                     GetURL("replaced_state"),
                                     GetURL("replaced_state")));

  // Back from `replaced_state` (for Main+Subframe). The previous entry
  // `replaced_state` (for main frame) is skipped, because it is both created by
  // ad and an ad entry creator.
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GURL(url::kAboutBlankURL));

  // Close browser to trigger metric recording.
  CloseBrowserSynchronously(browser());

  // UKM metrics are attributed to the initial URL (`frame_factory.html`) of
  // page where the back navigation started.
  ASSERT_TRUE(
      RecordedUkmUseCounter(blink::mojom::WebFeature::kHistoryGoBackWouldSkipAd,
                            GetURL("frame_factory.html")));
}

IN_PROC_BROWSER_TEST_F(AdNavigationEntryBrowserTest,
                       AdReplaceStateFollowedByAdFrameLocationAssign) {
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('replaced_state');
  )"));

  // Create an ad subframe.
  GURL ad_url = GetURL("frame_factory.html?2&ad=true");
  content::RenderFrameHost* ad_frame = CreateSrcFrame(GetWebContents(), ad_url);

  // Navigate the ad subframe via location.assign.
  GURL new_url = GetURL("test_div.html");
  content::TestNavigationManager navigation_manager(GetWebContents(), new_url);
  ASSERT_TRUE(content::ExecJs(ad_frame, content::JsReplace(R"(
    executeLocationAssignFromAdScript($1);
  )",
                                                           new_url)));
  EXPECT_TRUE(navigation_manager.WaitForNavigationFinished());

  EXPECT_THAT(GetNavigationUrls(),
              ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                                     GetURL("replaced_state"),
                                     GetURL("replaced_state")));

  // The cross-document, ad subframe navigation should flag the skippable ad
  // state of the originating history entry.
  //
  // Back from `replaced_state` (for Main+Subframe). The previous entry
  // `replaced_state` (for main frame) is skipped, because it is both created by
  // ad and an ad entry creator.
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GURL(url::kAboutBlankURL));
}

IN_PROC_BROWSER_TEST_F(AdNavigationEntryBrowserTest, ConsecutiveAdPushState) {
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryPushStateFromAdScript('new_state1');
    executeHistoryPushStateFromAdScript('new_state2');
    executeHistoryPushStateFromAdScript('new_state3');
  )"));

  EXPECT_THAT(
      GetNavigationUrls(),
      ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                             GetURL("frame_factory.html"), GetURL("new_state1"),
                             GetURL("new_state2"), GetURL("new_state3")));

  // Back from `new_state3`. The previous two entries (`new_state2` and
  // `new_state1`) are ad related entries.
  //
  // However, the start (`new_state3`) and target (`frame_factory.html`) entries
  // are same origin. Thus, we won't execute the ad skipping logic. Expect
  // normal back navigation to `new_state2`.
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GetURL("new_state2"));

  // Back from `new_state2`. For the same reason, we won't execute the ad
  // skipping logic. Expect normal back navigation to `new_state1`.
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GetURL("new_state1"));

  // Forward from `new_state1`. For the same reason, we won't execute the ad
  // skipping logic. Expect normal back navigation to `new_state2`.
  GoForward();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GetURL("new_state2"));

  // Close browser to trigger metric recording.
  CloseBrowserSynchronously(browser());

  // UKM metrics are attributed to the initial URL (`frame_factory.html`) of
  // page where the back/forward navigation started.
  ASSERT_TRUE(RecordedUkmUseCounter(
      blink::mojom::WebFeature::
          kHistoryGoBackWouldNotSkipAdDueToSameOriginExclusion,
      GetURL("frame_factory.html")));
  ASSERT_TRUE(RecordedUkmUseCounter(
      blink::mojom::WebFeature::
          kHistoryGoForwardWouldNotSkipAdDueToSameOriginExclusion,
      GetURL("frame_factory.html")));
}

IN_PROC_BROWSER_TEST_F(AdNavigationEntryBrowserTest,
                       SkippableAdStateResetByNonAdReplaceState) {
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('replaced_state1');
    executeHistoryPushStateFromAdScript('new_state');
    history.back();
  )"));

  EXPECT_THAT(
      GetNavigationUrls(),
      ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                             GetURL("replaced_state1"), GetURL("new_state")));

  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
      history.replaceState({}, '', 'replaced_state2');
    )"));

  EXPECT_THAT(
      GetNavigationUrls(),
      ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                             GetURL("replaced_state2"), GetURL("new_state")));

  // Forward to `new_state`.
  GoForward();

  // The last replaceState() call from a non-ad script should reset the
  // skippable ad state of the history entry.
  //
  // Back from `new_state`. The previous entry `replaced_state2` is not skipped.
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GetURL("replaced_state2"));

  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('replaced_state3');
  )"));

  EXPECT_THAT(
      GetNavigationUrls(),
      ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                             GetURL("replaced_state3"), GetURL("new_state")));

  // Forward to `new_state`.
  GoForward();

  // The last replaceState() call from an ad script should flag the skippable ad
  // state of the history entry. This confirms that the previous non-ad call
  // only reset the entry's `entry_created_by_ad` status, but not the
  // `ad_entry_creator` status.
  //
  // Back from `new_state`. The previous entry `replaced_state3` is skipped.
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GURL(url::kAboutBlankURL));
}

IN_PROC_BROWSER_TEST_F(AdNavigationEntryBrowserTest,
                       SkippableAdStateResetByNonAdLocationReplace) {
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('replaced_state1');
    executeHistoryPushStateFromAdScript('new_state');
    history.back();
  )"));

  GURL new_url = GetURL("frame_factory.html");
  content::TestNavigationManager navigation_manager(GetWebContents(), new_url);
  ASSERT_TRUE(content::ExecJs(GetWebContents(), content::JsReplace(R"(
    location.replace($1);
  )",
                                                                   new_url)));
  EXPECT_TRUE(navigation_manager.WaitForNavigationFinished());

  // Wait until the ad script has loaded and defined our function.
  const std::string wait_for_function_script = R"(
    new Promise(resolve => {
      const intervalId = setInterval(() => {
        if (typeof executeHistoryPushStateFromAdScript === 'function') {
          clearInterval(intervalId);
          resolve(true);
        }
      }, 10); // Poll every 10ms.
    })
  )";
  ASSERT_TRUE(content::EvalJs(GetWebContents(), wait_for_function_script)
                  .ExtractBool());

  EXPECT_THAT(GetNavigationUrls(),
              ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                                     GetURL("frame_factory.html"),
                                     GetURL("new_state")));

  // Forward to `new_state`.
  GoForward();

  // The last location.replace call from a non-ad script should reset the
  // skippable ad state of the history entry.
  //
  // Back from `new_state`. The previous entry `frame_factory.html` is not
  // skipped.
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(),
            GetURL("frame_factory.html"));
}

IN_PROC_BROWSER_TEST_F(AdNavigationEntryBrowserTest,
                       SkippableAdStateNotResetByNonAdPushState) {
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('replaced_state');
    executeHistoryPushStateFromAdScript('new_state1');
    history.back();
  )"));

  EXPECT_THAT(
      GetNavigationUrls(),
      ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                             GetURL("replaced_state"), GetURL("new_state1")));

  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    history.pushState({}, '', 'new_state2');
  )"));

  EXPECT_THAT(
      GetNavigationUrls(),
      ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                             GetURL("replaced_state"), GetURL("new_state2")));

  // A pushState() from a non-ad script should not reset the skippable ad state
  // of the originating history entry.
  //
  // Back from `new_state2`. The previous entry `replaced_state` is skipped.
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GURL(url::kAboutBlankURL));
}

IN_PROC_BROWSER_TEST_F(AdNavigationEntryBrowserTest,
                       SkippableAdStateNotResetByNonAdLocationAssign) {
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('replaced_state');
    executeHistoryPushStateFromAdScript('new_state');
    history.back();
  )"));

  GURL new_url = GetURL("frame_factory.html");
  content::TestNavigationManager navigation_manager(GetWebContents(), new_url);
  ASSERT_TRUE(content::ExecJs(GetWebContents(), content::JsReplace(R"(
    location.assign($1);
  )",
                                                                   new_url)));
  EXPECT_TRUE(navigation_manager.WaitForNavigationFinished());

  EXPECT_THAT(GetNavigationUrls(),
              ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                                     GetURL("replaced_state"),
                                     GetURL("frame_factory.html")));

  // A cross-document navigation from a non-ad script should not reset the
  // skippable ad state of the originating history entry.
  //
  // Back from `frame_factory.html`. The previous entry `replaced_state` is
  // skipped.
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GURL(url::kAboutBlankURL));
}

IN_PROC_BROWSER_TEST_F(AdNavigationEntryBrowserTest,
                       AdStatePreservedOnTabDuplication) {
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('replaced_state');
    executeHistoryPushStateFromAdScript('new_state');
  )"));

  EXPECT_THAT(
      GetNavigationUrls(),
      ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                             GetURL("replaced_state"), GetURL("new_state")));

  // Duplicate the tab.
  int original_index = browser()->tab_strip_model()->active_index();
  chrome::DuplicateTab(browser());

  // Verify the NEW tab is active and distinct.
  int new_index = browser()->tab_strip_model()->active_index();
  ASSERT_NE(original_index, new_index);

  // Note: These helper methods use GetWebContents(), which grabs the *active*
  // web contents. Since DuplicateTab activates the new tab, these calls check
  // the new tab.
  EXPECT_THAT(
      GetNavigationUrls(),
      ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                             GetURL("replaced_state"), GetURL("new_state")));

  // Back from `new_state`. The previous entry `replaced_state` is skipped.
  // This indicates that the ad skippable state is preserved on the DUPLICATED
  // tab.
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GURL(url::kAboutBlankURL));
}

// Regression test ensuring the navigation entry ad tagging heuristic ignores
// common monkey patches (replaceState/pushState) and successfully traces back
// to the true originator of the API call.
IN_PROC_BROWSER_TEST_F(
    AdNavigationEntryBrowserTest,
    AdReplaceStateFollowedByAdPushState_AdScriptCallsNonAdPatch) {
  // Monkey patch history.replaceState and history.pushState. This simulates
  // a non-ad script (e.g., a publisher script) proxying the history API calls.
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    const originalReplaceState = window.history.replaceState;
    window.history.replaceState = function(...args) {
      originalReplaceState.apply(window.history, args);
    };
    const originalPushState = window.history.pushState;
    window.history.pushState = function(...args) {
      originalPushState.apply(window.history, args);
    };
  )"));

  // Call the monkey-patched APIs from an ad script.
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('replaced_state');
    executeHistoryPushStateFromAdScript('new_state');
  )"));

  EXPECT_THAT(
      GetNavigationUrls(),
      ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                             GetURL("replaced_state"), GetURL("new_state")));

  // Verify that the entry is correctly tagged and skipped by the Back-To-Ad
  // intervention.
  // Back from 'new_state'. The previous entry `replaced_state` should be
  // skipped, even though it was directly called via a non-ad monkey patch.
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GURL(url::kAboutBlankURL));

  // Forward from `kAboutBlankURL` should also skip `replaced_state`.
  GoForward();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GetURL("new_state"));
}

IN_PROC_BROWSER_TEST_F(AdNavigationEntryBrowserTest,
                       OriginalSkippableEntriesFollowedByAdEntries) {
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetWebContents(), GetURL("test_div.html")));
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetWebContents(), GetURL("frame_factory.html")));

  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('new_state1');
    executeHistoryPushStateFromAdScript('new_state2');
    executeHistoryPushStateFromAdScript('new_state3');
  )"));

  EXPECT_THAT(GetNavigationUrls(),
              ::testing::ElementsAre(
                  GURL(url::kAboutBlankURL), GetURL("frame_factory.html"),
                  GetURL("test_div.html"), GetURL("new_state1"),
                  GetURL("new_state2"), GetURL("new_state3")));

  // Back from `new_state3`. It should skip both skippable entries determined by
  // original history intervention (frame_factory.html, test_div.html), as well
  // as ad entries (new_state1, new_state2).
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GURL(url::kAboutBlankURL));
}

IN_PROC_BROWSER_TEST_F(AdNavigationEntryBrowserTest,
                       AdEntriesFollowedByOriginalSkippableEntries) {
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('new_state1');
    executeHistoryPushStateFromAdScript('new_state2');
    executeHistoryPushStateFromAdScript('new_state3');
    executeHistoryPushStateFromAdScript('new_state4');
    history.back();
  )"));

  // Navigate to a cross-origin site so that the user gesture isn't inherited by
  // the new page.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetWebContents(), GetURL("b.com", "test_div.html")));
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetWebContents(), GetURL("b.com", "frame_factory.html")));

  EXPECT_THAT(
      GetNavigationUrls(),
      ::testing::ElementsAre(GURL(url::kAboutBlankURL), GetURL("new_state1"),
                             GetURL("new_state2"), GetURL("new_state3"),
                             GetURL("b.com", "test_div.html"),
                             GetURL("b.com", "frame_factory.html")));

  // Back from `frame_factory.html`. It should skip both skippable entries
  // determined by original history intervention (test_div.html), as well as ad
  // entries (new_state1, new_state2, new_state3).
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GURL(url::kAboutBlankURL));

  // Close browser to trigger metric recording.
  CloseBrowserSynchronously(browser());

  // UKM metrics are attributed to the page where the back navigation started
  // (`frame_factory.html`).
  ASSERT_TRUE(
      RecordedUkmUseCounter(blink::mojom::WebFeature::kHistoryGoBackWouldSkipAd,
                            GetURL("b.com", "frame_factory.html")));
}

class BackToAdInterventionDisabledBrowserTest
    : public AdNavigationEntryBrowserTest {
 public:
  BackToAdInterventionDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kBackToAdIntervention);
  }

  ~BackToAdInterventionDisabledBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BackToAdInterventionDisabledBrowserTest,
                       AdReplaceStateFollowedByAdPushState) {
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('replaced_state');
    executeHistoryPushStateFromAdScript('new_state');
  )"));

  EXPECT_THAT(
      GetNavigationUrls(),
      ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                             GetURL("replaced_state"), GetURL("new_state")));

  // Back from 'new_state'. Since the feature is DISABLED, we expect standard
  // behavior (no skipping) even though `replaced_state` is an ad.
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GetURL("replaced_state"));
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GURL(url::kAboutBlankURL));

  // Forward from 'kAboutBlankURL'. Similarly, we expect standard behavior (no
  // skipping).
  GoForward();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GetURL("replaced_state"));
  GoForward();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GetURL("new_state"));

  // Close browser to trigger metric recording.
  CloseBrowserSynchronously(browser());

  // Even though the skip did not occur (due to the feature being disabled),
  // the metrics should still be recorded indicating that the back-to-ad
  // intervention WOULD have an impact.
  //
  // UKM metrics are attributed to the initial URL (`frame_factory.html`) of
  // page where the back navigation started.
  ASSERT_TRUE(
      RecordedUkmUseCounter(blink::mojom::WebFeature::kHistoryGoBackWouldSkipAd,
                            GetURL("frame_factory.html")));
}

IN_PROC_BROWSER_TEST_F(BackToAdInterventionDisabledBrowserTest,
                       ConsecutiveAdPushState) {
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryPushStateFromAdScript('new_state1');
    executeHistoryPushStateFromAdScript('new_state2');
    executeHistoryPushStateFromAdScript('new_state3');
  )"));

  EXPECT_THAT(
      GetNavigationUrls(),
      ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                             GetURL("frame_factory.html"), GetURL("new_state1"),
                             GetURL("new_state2"), GetURL("new_state3")));

  // Go back twice and then forward. Expect standard behavior, as the
  // intervention is disabled.
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GetURL("new_state2"));
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GetURL("new_state1"));
  GoForward();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GetURL("new_state2"));

  // Close browser to trigger metric recording.
  CloseBrowserSynchronously(browser());

  // Even though the feature is disabled, the metrics should still be recorded
  // indicating that the same-origin exclusion WOULD have happened.
  //
  // UKM metrics are attributed to the initial URL (`frame_factory.html`) of
  // page where the back/forward navigation started.
  ASSERT_TRUE(RecordedUkmUseCounter(
      blink::mojom::WebFeature::
          kHistoryGoBackWouldNotSkipAdDueToSameOriginExclusion,
      GetURL("frame_factory.html")));
  ASSERT_TRUE(RecordedUkmUseCounter(
      blink::mojom::WebFeature::
          kHistoryGoForwardWouldNotSkipAdDueToSameOriginExclusion,
      GetURL("frame_factory.html")));
}

IN_PROC_BROWSER_TEST_F(BackToAdInterventionDisabledBrowserTest,
                       AdReplaceStateFollowedByAdPushState_NewTab) {
  // Open a new foreground tab. This gives us a pristine history stack that
  // starts at frame_factory.html.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetURL("frame_factory.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Quick sanity check to ensure our new active tab only has 1 history entry.
  ASSERT_EQ(GetWebContents()->GetController().GetEntryCount(), 1);

  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('replaced_state');
    executeHistoryPushStateFromAdScript('new_state');
  )"));

  EXPECT_THAT(
      GetNavigationUrls(),
      ::testing::ElementsAre(GetURL("replaced_state"), GetURL("new_state")));

  // Back from 'new_state'. Since the feature is DISABLED, we expect standard
  // behavior (no skipping) even though `replaced_state` is an ad.
  GoBack();
  EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), GetURL("replaced_state"));

  // Close browser to trigger metric recording.
  CloseBrowserSynchronously(browser());

  // Even though the skip did not occur (due to the feature being disabled),
  // the metrics should still be recorded indicating that the back-to-ad
  // intervention WOULD have an impact.
  //
  // Note: This contrasts with the base `AdReplaceStateFollowedByAdPushState`
  // test because here 'replaced_state' is the ONLY prior entry. If the feature
  // were enabled, the  `CanGoBack` capability check would fail and prevent the
  // back button navigation entirely.
  //
  // UKM metrics are attributed to the initial URL (`frame_factory.html`) of
  // page where the back navigation started.
  ASSERT_TRUE(
      RecordedUkmUseCounter(blink::mojom::WebFeature::kHistoryGoBackWouldSkipAd,
                            GetURL("frame_factory.html")));
}

}  // namespace subresource_filter
