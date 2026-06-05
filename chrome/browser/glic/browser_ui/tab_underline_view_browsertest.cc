// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/tab_underline_view.h"

#include "base/strings/strcat.h"
#include "chrome/browser/glic/browser_ui/tab_underline_controller.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab/alert_indicator_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "gpu/config/gpu_switches.h"
#include "ui/gfx/switches.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace glic {

namespace {

static constexpr float kFloatComparisonTolerance = 0.001f;

class TesterImpl : public TabUnderlineView::Tester {
 public:
  TesterImpl() = default;
  TesterImpl(const TesterImpl&) = delete;
  TesterImpl& operator=(const TesterImpl&) = delete;
  ~TesterImpl() override = default;

  // `TabUnderlineView::Tester`:
  base::TimeTicks GetTestTimestamp() override { return next_time_tick_; }
  base::TimeTicks GetTestCreationTime() override { return creation_time_; }
  void AnimationStarted() override {
    animation_started_ = true;
    wait_for_animation_started_.Quit();
  }
  void AnimationReset() override {
    animation_reset_ = true;
    wait_for_animation_reset_.Quit();
  }
  void RampDownStarted() override {
    ramp_down_started_ = true;
    wait_for_ramp_down_started_.Quit();
  }

  void set_underline(TabUnderlineView* underline) { underline_ = underline; }

  void ResetWaitForAnimationStart() { animation_started_ = false; }
  void WaitForAnimationStart() {
    if (animation_started_) {
      return;
    }
    SCOPED_TRACE("WaitForAnimationStart");
    wait_for_animation_started_.Run();
  }

  void WaitForAnimationReset() {
    if (animation_reset_) {
      return;
    }
    SCOPED_TRACE("WaitForAnimationReset");
    wait_for_animation_reset_.Run();
  }

  void WaitForRampDownStarted() {
    if (ramp_down_started_) {
      return;
    }
    SCOPED_TRACE("WaitForRampDownStarted");
    wait_for_ramp_down_started_.Run();
  }

  // Flush out the ramp down animation.
  void FinishRampDown() {
    // First call records the T0 for ramping down.
    AdvanceTimeAndTickAnimation(base::TimeDelta());
    AdvanceTimeAndTickAnimation(base::Seconds(2));
  }

  void AdvanceTimeAndTickAnimation(base::TimeDelta delta) {
    static constexpr base::TimeTicks kDummyTimeStamp;
    ASSERT_TRUE(underline_);
    next_time_tick_ += delta;
    underline_->OnAnimationStep(kDummyTimeStamp);
  }

 private:
  const base::TimeTicks creation_time_ = base::TimeTicks::Now();
  raw_ptr<TabUnderlineView> underline_ = nullptr;
  base::TimeTicks next_time_tick_ = creation_time_;

  bool animation_started_ = false;
  base::RunLoop wait_for_animation_started_;

  bool animation_reset_ = false;
  base::RunLoop wait_for_animation_reset_;

  bool ramp_down_started_ = false;
  base::RunLoop wait_for_ramp_down_started_;
};

class TestUnderlineView : public TabUnderlineView {
 public:
  TestUnderlineView(std::unique_ptr<TabUnderlineController> controller,
                    BrowserWindowInterface* browser_window_interface,
                    tabs::TabHandle handle,
                    std::unique_ptr<Tester> tester)
      : TabUnderlineView(std::move(controller),
                         browser_window_interface,
                         handle,
                         std::move(tester)) {}
  ~TestUnderlineView() override = default;
};

class TestFactory : public TabUnderlineView::Factory {
 public:
  TestFactory() { TabUnderlineView::Factory::set_factory(this); }
  ~TestFactory() override { TabUnderlineView::Factory::set_factory(nullptr); }

 protected:
  std::unique_ptr<TabUnderlineView> CreateUnderlineView(
      std::unique_ptr<TabUnderlineController> controller,
      BrowserWindowInterface* browser_window_interface,
      tabs::TabHandle handle) override {
    TabUnderlineView* new_underline =
        new TestUnderlineView(std::move(controller), browser_window_interface,
                              handle, std::make_unique<TesterImpl>());
    TesterImpl* tester = static_cast<TesterImpl*>(new_underline->tester());
    tester->set_underline(new_underline);
    return base::WrapUnique(new_underline);
  }
};

class TabUnderlineViewBrowserTest : public GlicBrowserTest {
 public:
  using PlatformBrowserTest::browser;
  using PlatformBrowserTest::CreateBrowser;
  using PlatformBrowserTest::CreateIncognitoBrowser;

  TabUnderlineViewBrowserTest() {
    const std::string multitab_feature_name =
        mojom::features::kGlicMultiTab.name;
    const std::string underline_feature_name =
        features::kGlicMultitabUnderlines.name;
    // Toggling `UiGpuRasterization` is only possible via command line.
    const std::string enabled_features =
        base::StrCat({multitab_feature_name, ",", underline_feature_name, ",",
                      "UiGpuRasterization"});
    features_.InitFromCommandLine(enabled_features,
                                  /*disabled_features=*/
                                  "ContextualTasks,GlicForceSimplifiedBorder,"
                                  "GlicForceNonSkSLBorder");
  }
  ~TabUnderlineViewBrowserTest() override = default;

  void SetUpOnMainThread() override {
    GlicBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), Title1()));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kForcePrefersNoReducedMotion);

    // This ensures that gpu rasterization (i.e hardware acceleration) is
    // available regardless of device. (This is required for underline view to
    // animate - See `AnimatedEffectView::ForceSimplifiedShader()`)
    command_line->AppendSwitch(switches::kIgnoreGpuBlocklist);
    GlicBrowserTest::SetUpCommandLine(command_line);
  }

  GURL Title1() const { return embedded_test_server()->GetURL("/title1.html"); }

  GURL Title2() const { return embedded_test_server()->GetURL("/title2.html"); }

  TabUnderlineView* GetUnderlineOfTab(Browser* target_browser, int index) {
    TabStripRegionView* tab_strip_view =
        target_browser->window()->AsBrowserView()->tab_strip_view();
    views::View* underline =
        tab_strip_view->GetTabAnchorViewAt(index)->GetViewByElementId(
            TabUnderlineView::kGlicTabUnderlineElementId);
    CHECK(underline);
    return views::AsViewClass<TabUnderlineView>(underline);
  }

  TabUnderlineView* GetUnderlineOfActiveTab(Browser* target_browser = nullptr) {
    if (!target_browser) {
      target_browser = browser();
    }
    return GetUnderlineOfTab(
        target_browser,
        TabListInterface::From(target_browser)->GetActiveIndex());
  }

  AlertIndicatorButton* GetAlertIndicatorButtonOfActiveTab() {
    TabStripRegionView* tab_strip_view =
        static_cast<BrowserView*>(browser()->window())->tab_strip_view();
    views::View* button =
        tab_strip_view
            ->GetTabAnchorViewAt(GetTabListInterface()->GetActiveIndex())
            ->GetViewByElementId(kTabAlertIndicatorButtonElementId);
    return views::AsViewClass<AlertIndicatorButton>(button);
  }

 private:
  base::test::ScopedFeatureList features_;
  TestFactory test_factory_;
};
}  // namespace

// Exercise the default user journey: toggles the underline animation and waits
// for it to finish.
IN_PROC_BROWSER_TEST_F(TabUnderlineViewBrowserTest, SmokeTest) {
  auto* underline = GetUnderlineOfActiveTab();
  ASSERT_TRUE(underline);
  TesterImpl* tester = static_cast<TesterImpl*>(underline->tester());

  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  instance->host().SetContextAccessIndicator(true);
  tester->WaitForAnimationStart();
  EXPECT_TRUE(underline->IsShowing());

  // Manually stepping the animation code to mimic the behavior of the
  // compositor. These tests follow the same setup as
  // ContextSharingBorderViewUiTest. For those tests, crbug.com/384712084 and
  // crbug.com/387386303 outline how testing via requesting screenshot from the
  // browser window was explored but ultimately failed due to test flakiness.
  // These tests follow the same workarounds, improvement is outlined in
  // TODO(crbug.com/crbug.com/387386303).

  // T=0s.
  tester->AdvanceTimeAndTickAnimation(base::TimeDelta());
  EXPECT_NEAR(underline->opacity_for_testing(), 0.f, kFloatComparisonTolerance);
  EXPECT_NEAR(underline->progress_for_testing(), 0.f,
              kFloatComparisonTolerance);

  // T=0.333s.
  tester->AdvanceTimeAndTickAnimation(base::Seconds(0.333));
  // 0.333/0.5.
  EXPECT_NEAR(underline->opacity_for_testing(), 0.666,
              kFloatComparisonTolerance);
  // 0.333/3
  EXPECT_NEAR(underline->progress_for_testing(), 0.111f,
              kFloatComparisonTolerance);

  // T=1.333s
  tester->AdvanceTimeAndTickAnimation(base::Seconds(1));
  // Opacity ramp up is 0.5s.
  EXPECT_NEAR(underline->opacity_for_testing(), 1.f, kFloatComparisonTolerance);
  // 1.333/3
  EXPECT_NEAR(underline->progress_for_testing(), 0.444f,
              kFloatComparisonTolerance);

  // T=2.433s
  tester->AdvanceTimeAndTickAnimation(base::Seconds(1.1));
  EXPECT_NEAR(underline->opacity_for_testing(), 1.f, kFloatComparisonTolerance);
  // 2.433/3
  EXPECT_NEAR(underline->progress_for_testing(), 0.811,
              kFloatComparisonTolerance);

  ASSERT_OK(CloseGlicForTabAndWait(GetTabListInterface()->GetActiveTab()));
  tester->WaitForRampDownStarted();
  tester->FinishRampDown();
  EXPECT_FALSE(underline->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabUnderlineViewBrowserTest,
                       ToggleSharingWithSingleTab) {
  auto* underline = GetUnderlineOfActiveTab();
  TesterImpl* tester = static_cast<TesterImpl*>(underline->tester());
  EXPECT_FALSE(underline->IsShowing());

  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab())
  EXPECT_TRUE(coordinator().IsAnyPanelShowing());
  // The underline should show when sharing is turned on.
  ASSERT_OK(WaitForGlicClient(instance));
  instance->host().SetContextAccessIndicator(true);

  tester->WaitForAnimationStart();
  EXPECT_TRUE(underline->IsShowing());
  tester->AdvanceTimeAndTickAnimation(base::TimeDelta());
  tester->AdvanceTimeAndTickAnimation(base::Seconds(0.3));
}

IN_PROC_BROWSER_TEST_F(TabUnderlineViewBrowserTest,
                       SingleTabPinningWhileGlicWindowOpen) {
  ASSERT_OK(OpenGlicForActiveTab());
  EXPECT_TRUE(coordinator().IsAnyPanelShowing());
  auto* underline = GetUnderlineOfActiveTab();
  TesterImpl* tester = static_cast<TesterImpl*>(underline->tester());
  // Tab is pinned by default.
  EXPECT_TRUE(underline->IsShowing());
  tester->AdvanceTimeAndTickAnimation(base::TimeDelta());
  tester->AdvanceTimeAndTickAnimation(base::Seconds(0.3));

  // The underline should hide when its tab is unpinned.
  auto& global_sharing_manager = service()->active_instance_sharing_manager();
  global_sharing_manager.UnpinAllTabs();
  ASSERT_FALSE(global_sharing_manager.IsTabPinned(
      GetTabListInterface()->GetTab(0)->GetHandle()));
  tester->WaitForRampDownStarted();
  tester->FinishRampDown();
  EXPECT_FALSE(underline->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabUnderlineViewBrowserTest,
                       ToggleGlicWindowVisibilityWithPinnedTab) {
  auto* underline = GetUnderlineOfActiveTab();
  TesterImpl* tester = static_cast<TesterImpl*>(underline->tester());

  // The underline of a pinned tab should show when the glic window is opened.
  ASSERT_OK(OpenGlicForActiveTab());
  tester->WaitForAnimationStart();
  EXPECT_TRUE(underline->IsShowing());
  tester->AdvanceTimeAndTickAnimation(base::TimeDelta());
  tester->AdvanceTimeAndTickAnimation(base::Seconds(0.3));

  // The underline of a pinned tab should hide when the glic window is closed.
  ASSERT_OK(CloseGlicForTabAndWait(GetTabListInterface()->GetActiveTab()));
  tester->WaitForRampDownStarted();
  tester->FinishRampDown();
  EXPECT_FALSE(underline->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabUnderlineViewBrowserTest, FocusedTabChange) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  auto* underline1 = GetUnderlineOfActiveTab();

  // Add second tab.
  CreateAndActivateTab(Title2());
  auto* underline2 = GetUnderlineOfActiveTab();

  // The underline of the active tab should show when sharing is turned on.
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  instance->host().SetContextAccessIndicator(true);
  EXPECT_TRUE(underline2->IsShowing());

  GetTabListInterface()->ActivateTab(tab1->GetHandle());
  instance->Show(ShowOptions::ForSidePanel(
      *tab1, mojom::InvocationSource::kTopChromeButton));
  EXPECT_TRUE(underline1->IsShowing());
  EXPECT_TRUE(underline2->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabUnderlineViewBrowserTest,
                       FocusedTabChangeBetweenPinnedTabs) {
  // Open two tabs, and bind/pin them to the same glic instance.
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  auto* underline1 = GetUnderlineOfActiveTab();
  TesterImpl* tester1 = static_cast<TesterImpl*>(underline1->tester());

  CreateAndActivateTab(Title2());
  auto* underline2 = GetUnderlineOfActiveTab();
  TesterImpl* tester2 = static_cast<TesterImpl*>(underline2->tester());

  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  instance->host().SetContextAccessIndicator(true);
  instance->Show(ShowOptions::ForSidePanel(
      *tab1, mojom::InvocationSource::kTopChromeButton));

  // Underlines of all pinned tabs should show when the glic window is opened.
  if (!underline1->IsShowing()) {
    tester1->WaitForAnimationStart();
  }
  if (!underline2->IsShowing()) {
    tester2->WaitForAnimationStart();
  }
  EXPECT_TRUE(underline1->IsShowing());
  EXPECT_TRUE(underline2->IsShowing());
  // Allow animations to reach their steady states.
  tester1->AdvanceTimeAndTickAnimation(base::TimeDelta());
  tester1->AdvanceTimeAndTickAnimation(base::Seconds(3));
  tester2->AdvanceTimeAndTickAnimation(base::TimeDelta());
  tester2->AdvanceTimeAndTickAnimation(base::Seconds(3));

  // Grab current animation values for later comparison.
  float u1_opacity = underline1->opacity_for_testing();
  float u2_opacity = underline2->opacity_for_testing();

  // While sharing is off, changing focus between pinned tabs should have no
  // visual effect on their underlines.
  GetTabListInterface()->ActivateTab(tab1->GetHandle());
  tester1->AdvanceTimeAndTickAnimation(base::TimeDelta());
  tester2->AdvanceTimeAndTickAnimation(base::TimeDelta());

  EXPECT_TRUE(underline1->IsShowing());
  EXPECT_TRUE(underline2->IsShowing());
  EXPECT_EQ(underline1->opacity_for_testing(), u1_opacity);
  EXPECT_EQ(underline2->opacity_for_testing(), u2_opacity);
}

IN_PROC_BROWSER_TEST_F(TabUnderlineViewBrowserTest, TabAlertIndicatorHidden) {
  auto* underline = GetUnderlineOfActiveTab();
  ASSERT_TRUE(underline);
  TesterImpl* tester = static_cast<TesterImpl*>(underline->tester());

  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  instance->host().SetContextAccessIndicator(true);
  tester->WaitForAnimationStart();
  EXPECT_TRUE(underline->IsShowing());

  // The shared tab should not have a visible tab alert indicator.
  EXPECT_FALSE(GetAlertIndicatorButtonOfActiveTab()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(TabUnderlineViewBrowserTest,
                       TabAlertIndicatorHidden_PinnedTab) {
  ASSERT_OK(OpenGlicForActiveTab());
  auto* underline = GetUnderlineOfActiveTab();
  TesterImpl* tester = static_cast<TesterImpl*>(underline->tester());
  EXPECT_TRUE(underline->IsShowing());

  // The pinned tab should not have a visible tab alert indicator.
  EXPECT_FALSE(GetAlertIndicatorButtonOfActiveTab()->GetVisible());
  tester->FinishRampDown();
}

IN_PROC_BROWSER_TEST_F(TabUnderlineViewBrowserTest, IncognitoModeCrash) {
  Browser* incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(incognito_browser, GURL("about:blank")));
}

// TODO(crbug.com/513374065): Re-enable this test once the flakiness is fixed on
// Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_AttachPinnedTabToNewWindow DISABLED_AttachPinnedTabToNewWindow
#else
#define MAYBE_AttachPinnedTabToNewWindow AttachPinnedTabToNewWindow
#endif
IN_PROC_BROWSER_TEST_F(TabUnderlineViewBrowserTest,
                       MAYBE_AttachPinnedTabToNewWindow) {
  // Set up two windows, each with one tab
  ASSERT_EQ(GetTabListInterface()->GetTabCount(), 1);
  // Second browser window
  Browser* browser2 = CreateBrowser(browser()->profile());
  browser2->GetWindow()->Activate();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser2, Title2()));
  ASSERT_EQ(GetTabListInterface()->GetTabCount(), 1);
  auto* tab_list2 = TabListInterface::From(browser2);
  ASSERT_EQ(tab_list2->GetTabCount(), 1);

  tabs::TabHandle handle1 = GetTabListInterface()->GetTab(0)->GetHandle();
  tabs::TabHandle handle2 = tab_list2->GetTab(0)->GetHandle();
  ASSERT_TRUE(handle1.Get());
  ASSERT_TRUE(handle2.Get());

  auto& global_sharing_manager = service()->active_instance_sharing_manager();
  EXPECT_TRUE(global_sharing_manager.GetPinnedTabs().empty());
  // Toggle Glic on second browser window to create an instance. Because the
  // second browser is active, the main sharing manager will delegate to this
  // instance's sharing manager.
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  // Pin both tabs on the instance's sharing manager.
  auto& instance_sharing_manager = instance->sharing_manager();
  instance_sharing_manager.PinTabs({handle1, handle2});
  EXPECT_TRUE(instance_sharing_manager.IsTabPinned(handle1));
  EXPECT_TRUE(instance_sharing_manager.IsTabPinned(handle2));

  // Verify the main sharing manager sees it (to show that delegation is
  // working).
  EXPECT_TRUE(global_sharing_manager.IsTabPinned(handle1));
  EXPECT_TRUE(global_sharing_manager.IsTabPinned(handle2));

  // Verify both tabs have underlines showing.
  auto* underline1 = GetUnderlineOfActiveTab(browser());
  auto* underline2 = GetUnderlineOfActiveTab(browser2);
  ASSERT_TRUE(underline1);
  ASSERT_TRUE(underline2);
  static_cast<TesterImpl*>(underline1->tester())->WaitForAnimationStart();
  static_cast<TesterImpl*>(underline2->tester())->WaitForAnimationStart();
  EXPECT_TRUE(underline1->IsShowing());
  EXPECT_TRUE(underline2->IsShowing());

  // Simulate attachment of browser2's tab to browser1.
  tab_list2->MoveTabToWindow(handle2, GetBrowser()->GetSessionID(), 1);
  EXPECT_EQ(GetTabListInterface()->GetTabCount(), 2);

  // Check that the newly attached tab has its underline showing.
  auto* underline_attached = GetUnderlineOfTab(browser(), 1);
  ASSERT_TRUE(underline_attached);
  static_cast<TesterImpl*>(underline_attached->tester())
      ->WaitForAnimationStart();
  EXPECT_TRUE(underline_attached->IsShowing());
}

}  // namespace glic
