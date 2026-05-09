// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_observer.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace glic {

class GlicNudgeControllerInteractiveUiTest : public test::InteractiveGlicTest {
 public:
  GlicNudgeControllerInteractiveUiTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{kUseAnchoredMessage,
                               contextual_cueing::kContextualCueingV2});
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    GlicEnabling::SetBypassEnablementChecksForTesting(true);
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip,
                                                 true);

    ASSERT_TRUE(tab_strip_action_container());
  }

  void TearDownOnMainThread() override {
    GlicEnabling::SetBypassEnablementChecksForTesting(false);
  }

  GlicNudgeController* nudge_controller() {
    return browser()->browser_window_features()->glic_nudge_controller();
  }

  TabStripActionContainer* tab_strip_action_container() {
    return BrowserElementsViews::From(browser())
        ->GetViewAs<TabStripActionContainer>(kTabStripActionContainerElementId);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicNudgeControllerInteractiveUiTest,
                       ShowsTabStripNudge) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_FALSE(tab_strip_action_container()->GetIsShowingGlicNudge());
  LOG(ERROR) << "asdf about to update\n";
  nudge_controller()->UpdateNudgeLabel(
      web_contents, "Nudge Label", "Prompt Suggestion", "Anchored Message Text",
      std::nullopt, base::DoNothing());

  EXPECT_TRUE(tab_strip_action_container()->GetIsShowingGlicNudge());
}

IN_PROC_BROWSER_TEST_F(GlicNudgeControllerInteractiveUiTest,
                       HidesTabStripNudge) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_FALSE(tab_strip_action_container()->GetIsShowingGlicNudge());
  nudge_controller()->UpdateNudgeLabel(
      web_contents, "Nudge Label", "Prompt Suggestion", "Anchored Message Text",
      std::nullopt, base::DoNothing());

  EXPECT_TRUE(tab_strip_action_container()->GetIsShowingGlicNudge());

  nudge_controller()->UpdateNudgeLabel(
      web_contents, std::string(), std::nullopt, std::string(),
      GlicNudgeActivity::kNudgeDismissed, base::DoNothing());
  EXPECT_FALSE(tab_strip_action_container()->GetIsShowingGlicNudge());
}

class GlicNudgeControllerAnchoredMessageInteractiveUiTest
    : public GlicNudgeControllerInteractiveUiTest {
 public:
  GlicNudgeControllerAnchoredMessageInteractiveUiTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{kUseAnchoredMessage},
        /*disabled_features=*/{contextual_cueing::kContextualCueingV2});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicNudgeControllerAnchoredMessageInteractiveUiTest,
                       ShowsAnchoredMessage) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  page_actions::PageActionController* page_action_controller =
      browser()
          ->GetActiveTabInterface()
          ->GetTabFeatures()
          ->page_action_controller();
  page_actions::PageActionObserver observer(kActionGlicContextualCueing);
  observer.RegisterAsPageActionObserver(*page_action_controller);

  nudge_controller()->UpdateNudgeLabel(
      web_contents, "Nudge Label", "Prompt Suggestion", "Anchored Message Text",
      std::nullopt, base::DoNothing());

  EXPECT_TRUE(observer.GetCurrentPageActionState().anchored_message_showing);
}

IN_PROC_BROWSER_TEST_F(GlicNudgeControllerAnchoredMessageInteractiveUiTest,
                       HidesAnchoredMessage) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  page_actions::PageActionController* page_action_controller =
      browser()
          ->GetActiveTabInterface()
          ->GetTabFeatures()
          ->page_action_controller();
  page_actions::PageActionObserver observer(kActionGlicContextualCueing);
  observer.RegisterAsPageActionObserver(*page_action_controller);

  nudge_controller()->UpdateNudgeLabel(
      web_contents, "Nudge Label", "Prompt Suggestion", "Anchored Message Text",
      std::nullopt, base::DoNothing());

  ASSERT_TRUE(observer.GetCurrentPageActionState().anchored_message_showing);

  nudge_controller()->UpdateNudgeLabel(
      web_contents, std::string(), std::nullopt, std::string(),
      GlicNudgeActivity::kNudgeDismissed, base::DoNothing());

  EXPECT_FALSE(observer.GetCurrentPageActionState().anchored_message_showing);
}

}  // namespace glic
