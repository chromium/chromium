// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_warming_checks.h"
#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_observer.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace glic {

// TODO(crbug.com/537847029): Migrate this test suite to GlicBrowserTest.
class GlicNudgeControllerInteractiveUiTest : public test::InteractiveGlicTest {
 public:
  GlicNudgeControllerInteractiveUiTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{contextual_cueing::kContextualCueingV2});
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    scoped_glic_bypass_.emplace();
    browser()->GetProfile()->GetPrefs()->SetBoolean(
        prefs::kGlicPinnedToTabstrip, true);

    ASSERT_TRUE(tab_strip_action_container());
  }

  void TearDownOnMainThread() override {
    scoped_glic_bypass_.reset();
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  GlicNudgeController* nudge_controller() {
    return browser()->GetFeatures().glic_nudge_controller();
  }

  TabStripActionContainer* tab_strip_action_container() {
    return BrowserElementsViews::From(browser())
        ->GetViewAs<TabStripActionContainer>(kTabStripActionContainerElementId);
  }

 private:
  std::optional<GlicEnabling::ScopedBypassEnablementChecksForTesting>
      scoped_glic_bypass_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicNudgeControllerInteractiveUiTest,
                       ShowsTabStripNudge) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_FALSE(tab_strip_action_container()->GetIsShowingGlicNudge());
  LOG(ERROR) << "asdf about to update\n";
  nudge_controller()->UpdateNudgeLabel(web_contents, "Nudge Label",
                                       "Prompt Suggestion", std::nullopt,
                                       base::DoNothing());

  EXPECT_TRUE(tab_strip_action_container()->GetIsShowingGlicNudge());
}

IN_PROC_BROWSER_TEST_F(GlicNudgeControllerInteractiveUiTest,
                       HidesTabStripNudge) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_FALSE(tab_strip_action_container()->GetIsShowingGlicNudge());
  nudge_controller()->UpdateNudgeLabel(web_contents, "Nudge Label",
                                       "Prompt Suggestion", std::nullopt,
                                       base::DoNothing());

  EXPECT_TRUE(tab_strip_action_container()->GetIsShowingGlicNudge());

  nudge_controller()->UpdateNudgeLabel(
      web_contents, std::string(), std::nullopt,
      GlicNudgeActivity::kNudgeDismissed, base::DoNothing());
  EXPECT_FALSE(tab_strip_action_container()->GetIsShowingGlicNudge());
}

class GlicNudgeControllerWarmingInteractiveUiTest
    : public GlicNudgeControllerInteractiveUiTest {
 public:
  GlicNudgeControllerWarmingInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicWarmOnNudge,
                              features::kGlicAnchorEntryPointForOnboardedUsers},
        /*disabled_features=*/{contextual_cueing::kContextualCueingV2,
                               features::kGlicWarming});
  }

  void SetUp() override {
    ForceConnectionTypeForTesting(
        net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
    GlicNudgeControllerInteractiveUiTest::SetUp();
  }

  void TearDown() override {
    GlicNudgeControllerInteractiveUiTest::TearDown();
    ForceConnectionTypeForTesting(std::nullopt);
  }

  bool IsWarmed() {
    auto* keyed_service = GlicKeyedService::Get(browser()->GetProfile());
    auto& coordinator = static_cast<GlicInstanceCoordinatorImpl&>(
        keyed_service->instance_coordinator());
    return coordinator.GetWebContentsWarmingPoolForTesting()
        .HasWarmedContainerForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicNudgeControllerWarmingInteractiveUiTest,
                       WarmsOnNudgeShownWhenFeatureEnabled) {
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(IsWarmed());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  nudge_controller()->UpdateNudgeLabel(web_contents, "Nudge Label",
                                       "Prompt Suggestion", std::nullopt,
                                       base::DoNothing());

  EXPECT_TRUE(tab_strip_action_container()->GetIsShowingGlicNudge());
  EXPECT_TRUE(base::test::RunUntil([this]() { return IsWarmed(); }));

  histogram_tester.ExpectBucketCount("Glic.Prewarming.ChecksResult.Nudge",
                                     GlicPrewarmingChecksResult::kSuccess, 1);
  histogram_tester.ExpectBucketCount(
      "Glic.WarmingPool.ContainerCreationReason",
      GlicWebContentsWarmingPool::ContainerCreationReason::kNudge, 1);
}

class GlicNudgeControllerWarmingDisabledInteractiveUiTest
    : public GlicNudgeControllerInteractiveUiTest {
 public:
  GlicNudgeControllerWarmingDisabledInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicAnchorEntryPointForOnboardedUsers},
        /*disabled_features=*/{contextual_cueing::kContextualCueingV2,
                               features::kGlicWarmOnNudge,
                               features::kGlicWarming});
  }

  void SetUp() override {
    ForceConnectionTypeForTesting(
        net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
    GlicNudgeControllerInteractiveUiTest::SetUp();
  }

  void TearDown() override {
    GlicNudgeControllerInteractiveUiTest::TearDown();
    ForceConnectionTypeForTesting(std::nullopt);
  }

  bool IsWarmed() {
    auto* keyed_service = GlicKeyedService::Get(browser()->GetProfile());
    auto& coordinator = static_cast<GlicInstanceCoordinatorImpl&>(
        keyed_service->instance_coordinator());
    return coordinator.GetWebContentsWarmingPoolForTesting()
        .HasWarmedContainerForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicNudgeControllerWarmingDisabledInteractiveUiTest,
                       DoesNotWarmOnNudgeShownWhenFeatureDisabled) {
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(IsWarmed());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  nudge_controller()->UpdateNudgeLabel(web_contents, "Nudge Label",
                                       "Prompt Suggestion", std::nullopt,
                                       base::DoNothing());

  EXPECT_TRUE(tab_strip_action_container()->GetIsShowingGlicNudge());

  base::PlatformThread::Sleep(base::Milliseconds(200));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsWarmed());

  histogram_tester.ExpectTotalCount("Glic.Prewarming.ChecksResult.Nudge", 0);
  histogram_tester.ExpectBucketCount(
      "Glic.WarmingPool.ContainerCreationReason",
      GlicWebContentsWarmingPool::ContainerCreationReason::kNudge, 0);
}

}  // namespace glic
