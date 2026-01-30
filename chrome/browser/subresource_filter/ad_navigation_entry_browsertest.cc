// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/subresource_filter/content/browser/ad_tagging_browser_test_utils.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/url_constants.h"

namespace subresource_filter {

// Tests the ad tagging logic for navigation entries to support the Ad History
// Intervention (which allows the browser to skip over ad entries).
//
// Note: This test suite lives in the chrome/ layer because the ad classifying
// functionality depends on Chrome-specific logic.
class AdNavigationEntryBrowserTest : public SubresourceFilterBrowserTest {
 public:
  ~AdNavigationEntryBrowserTest() override = default;

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();

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

  // Returns a vector indicating whether each entry in the current navigation
  // history is a candidate to be skipped due to the back-to-ad intervention
  // during back/forward UI navigation.
  std::vector<bool> GetSkippableAdState() {
    std::vector<bool> results;
    for (int i = 0; i < GetWebContents()->GetController().GetEntryCount();
         ++i) {
      results.push_back(GetWebContents()
                            ->GetController()
                            .GetEntryAtIndex(i)
                            ->IsPossiblySkippableAdEntryForTesting());
    }

    return results;
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
  EXPECT_THAT(GetSkippableAdState(),
              ::testing::ElementsAre(false, false, false));
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
  EXPECT_THAT(GetSkippableAdState(),
              ::testing::ElementsAre(false, false, false));
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
  EXPECT_THAT(GetSkippableAdState(),
              ::testing::ElementsAre(false, true, false));
}

IN_PROC_BROWSER_TEST_F(AdNavigationEntryBrowserTest,
                       AdReplaceStateFollowedByAdReplaceState) {
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('replaced_state1');
    executeHistoryReplaceStateFromAdScript('replaced_state2');
  )"));

  EXPECT_THAT(GetNavigationUrls(),
              ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                                     GetURL("replaced_state2")));

  // Invoking history.replaceState twice from an ad script should not flag the
  // entry as a candidate for skipping, as it lacks an ad-entry-creator signal.
  EXPECT_THAT(GetSkippableAdState(), ::testing::ElementsAre(false, false));
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
  EXPECT_THAT(GetSkippableAdState(),
              ::testing::ElementsAre(false, true, false));
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
  // the originating history entry.
  EXPECT_THAT(GetSkippableAdState(),
              ::testing::ElementsAre(false, false, false));
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
  // not flag the destination history entry.
  EXPECT_THAT(GetSkippableAdState(),
              ::testing::ElementsAre(false, false, false));
}

IN_PROC_BROWSER_TEST_F(AdNavigationEntryBrowserTest,
                       AdReplaceStateFollowedByAdFramePushState) {
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('replaced_state');
  )"));

  // Create an ad subframe.
  GURL ad_url = GetURL("frame_factory.html?2&ad=true");
  content::RenderFrameHost* ad_frame = CreateSrcFrame(GetWebContents(), ad_url);

  ASSERT_TRUE(content::ExecJs(ad_frame, R"(
    history.pushState({}, '', 'new_state')
  )"));

  EXPECT_THAT(GetNavigationUrls(),
              ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                                     GetURL("replaced_state"),
                                     GetURL("replaced_state")));
  EXPECT_THAT(GetSkippableAdState(),
              ::testing::ElementsAre(false, true, false));
}

IN_PROC_BROWSER_TEST_F(AdNavigationEntryBrowserTest,
                       AdReplaceStateFollowedByAdFrameLocationAssign) {
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('replaced_state');
  )"));

  // Create an ad subframe.
  GURL ad_url = GetURL("frame_factory.html?2&ad=true");
  content::RenderFrameHost* ad_frame = CreateSrcFrame(GetWebContents(), ad_url);

  // Navigation the ad subframe via location.assign.
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
  EXPECT_THAT(GetSkippableAdState(),
              ::testing::ElementsAre(false, true, false));
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
  EXPECT_THAT(GetSkippableAdState(),
              ::testing::ElementsAre(false, false, true, true, false));
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
  EXPECT_THAT(GetSkippableAdState(),
              ::testing::ElementsAre(false, true, false));

  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    history.replaceState({}, '', 'replaced_state2');
  )"));

  EXPECT_THAT(
      GetNavigationUrls(),
      ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                             GetURL("replaced_state2"), GetURL("new_state")));

  // A replaceState() call from a non-ad script should reset the skippable ad
  // state of the current history entry.
  EXPECT_THAT(GetSkippableAdState(),
              ::testing::ElementsAre(false, false, false));

  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('replaced_state3');
  )"));

  EXPECT_THAT(
      GetNavigationUrls(),
      ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                             GetURL("replaced_state3"), GetURL("new_state")));

  // A subsequent replaceState() call from an ad script should flag the
  // skippable ad state of the current history entry. This confirms that the
  // previous non-ad call only reset the entry's `entry_created_by_ad` status,
  // but not the `ad_entry_creator` status.
  EXPECT_THAT(GetSkippableAdState(),
              ::testing::ElementsAre(false, true, false));
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

  // A location.replace call from a non-ad script should reset the skippable ad
  // state of the current history entry.
  EXPECT_THAT(GetSkippableAdState(),
              ::testing::ElementsAre(false, false, false));
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
  EXPECT_THAT(GetSkippableAdState(),
              ::testing::ElementsAre(false, true, false));

  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    history.pushState({}, '', 'new_state2');
  )"));

  EXPECT_THAT(
      GetNavigationUrls(),
      ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                             GetURL("replaced_state"), GetURL("new_state2")));

  // A pushState() from a non-ad script should not reset the skippable ad state
  // of the originating history entry.
  EXPECT_THAT(GetSkippableAdState(),
              ::testing::ElementsAre(false, true, false));
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

  // A cross-document navigation from an non-ad script should not reset the
  // skippable ad state of the originating history entry.
  EXPECT_THAT(GetSkippableAdState(),
              ::testing::ElementsAre(false, true, false));
}

IN_PROC_BROWSER_TEST_F(AdNavigationEntryBrowserTest,
                       AdStatePreservedOnTabDuplication) {
  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"(
    executeHistoryReplaceStateFromAdScript('replaced_state');
    executeHistoryPushStateFromAdScript('new_state');
  )"));

  // Verify the state on the ORIGINAL tab.
  EXPECT_THAT(
      GetNavigationUrls(),
      ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                             GetURL("replaced_state"), GetURL("new_state")));
  EXPECT_THAT(GetSkippableAdState(),
              ::testing::ElementsAre(false, true, false));

  // Duplicate the tab.
  int original_index = browser()->tab_strip_model()->active_index();
  chrome::DuplicateTab(browser());

  // Verify the NEW tab is active and distinct.
  int new_index = browser()->tab_strip_model()->active_index();
  ASSERT_NE(original_index, new_index);

  // Verify state is preserved on the DUPLICATED tab.
  //
  // Note: These helper methods use GetWebContents(), which grabs the *active*
  // web contents. Since DuplicateTab activates the new tab, these calls check
  // the new tab.
  EXPECT_THAT(
      GetNavigationUrls(),
      ::testing::ElementsAre(GURL(url::kAboutBlankURL),
                             GetURL("replaced_state"), GetURL("new_state")));
  EXPECT_THAT(GetSkippableAdState(),
              ::testing::ElementsAre(false, true, false));
}

}  // namespace subresource_filter
