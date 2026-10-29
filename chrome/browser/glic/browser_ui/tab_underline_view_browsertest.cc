// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/tab_underline_view.h"

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/glic/browser_ui/tab_underline_controller.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab/alert_indicator_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "gpu/config/gpu_switches.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/switches.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

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
    ++animation_started_count_;
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
  int animation_started_count() const { return animation_started_count_; }
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
  int animation_started_count_ = 0;
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

  const std::vector<SkColor>& colors() const { return colors_; }
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

class TabUnderlineViewBrowserTestBase : public GlicBrowserTest {
 public:
  using PlatformBrowserTest::browser;
  using PlatformBrowserTest::CreateBrowser;
  using PlatformBrowserTest::CreateIncognitoBrowser;

  explicit TabUnderlineViewBrowserTestBase(bool enable_gpu_rasterization) {
    const std::string multitab_feature_name =
        mojom::features::kGlicMultiTab.name;
    const std::string underline_feature_name =
        features::kGlicMultitabUnderlines.name;
    // Toggling `UiGpuRasterization` is only possible via command line.
    std::string enabled_features =
        base::StrCat({multitab_feature_name, ",", underline_feature_name});
    std::string disabled_features =
        "ContextualTasks,GlicForceSimplifiedBorder,GlicForceNonSkSLBorder";
    if (enable_gpu_rasterization) {
      base::StrAppend(&enabled_features, {",UiGpuRasterization"});
    } else {
      base::StrAppend(&disabled_features, {",UiGpuRasterization"});
    }
    features_.InitFromCommandLine(enabled_features, disabled_features);
  }
  ~TabUnderlineViewBrowserTestBase() override = default;

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

  TabUnderlineView* GetUnderlineOfTab(BrowserWindowInterface* target_browser,
                                      int index) {
    TabStripRegionView* tab_strip_view =
        BrowserView::GetBrowserViewForBrowser(target_browser)->tab_strip_view();
    views::View* underline =
        tab_strip_view
            ->GetTabAnchorView(target_browser->tab_strip_model()
                                   ->GetTabAtIndex(index)
                                   ->GetHandle())
            ->GetViewByElementId(TabUnderlineView::kGlicTabUnderlineElementId);
    CHECK(underline);
    return views::AsViewClass<TabUnderlineView>(underline);
  }

  TabUnderlineView* GetUnderlineOfActiveTab(
      BrowserWindowInterface* target_browser = nullptr) {
    if (!target_browser) {
      target_browser = browser();
    }
    return GetUnderlineOfTab(
        target_browser,
        TabListInterface::From(target_browser)->GetActiveIndex());
  }

  AlertIndicatorButton* GetAlertIndicatorButtonOfActiveTab() {
    TabStripRegionView* tab_strip_view =
        BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view();
    views::View* button =
        tab_strip_view
            ->GetTabAnchorView(
                browser()->tab_strip_model()->GetActiveTab()->GetHandle())
            ->GetViewByElementId(kTabAlertIndicatorButtonElementId);
    return views::AsViewClass<AlertIndicatorButton>(button);
  }

  void VerifyRampUpWithSkippedAnimation() {
    auto* underline = GetUnderlineOfActiveTab();
    ASSERT_TRUE(underline);
    TesterImpl* tester = static_cast<TesterImpl*>(underline->tester());

    ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
    ASSERT_OK(WaitForGlicClient(instance));
    instance->host().SetContextAccessIndicator(true);
    tester->WaitForAnimationStart();
    EXPECT_TRUE(underline->IsShowing());
    ASSERT_TRUE(underline->layer());
    ASSERT_TRUE(underline->layer()->GetCompositor());
    EXPECT_TRUE(
        underline->layer()->GetCompositor()->HasAnimationObserver(underline));

    // T=0s.
    tester->AdvanceTimeAndTickAnimation(base::TimeDelta());
    EXPECT_NEAR(underline->opacity_for_testing(), 0.f,
                kFloatComparisonTolerance);
    EXPECT_NEAR(underline->progress_for_testing(), 0.f,
                kFloatComparisonTolerance);

    // T=0.123s.
    tester->AdvanceTimeAndTickAnimation(base::Seconds(0.123));
    // Fast opacity ramp up is 0.2s; 0.123/0.2=0.615.
    EXPECT_NEAR(underline->opacity_for_testing(), 0.615f,
                kFloatComparisonTolerance);
    EXPECT_NEAR(underline->progress_for_testing(), 0.f,
                kFloatComparisonTolerance);
    EXPECT_TRUE(
        underline->layer()->GetCompositor()->HasAnimationObserver(underline));

    // T=1s (steady state reached after 0.2s ramp up).
    tester->AdvanceTimeAndTickAnimation(base::Seconds(0.877));
    EXPECT_NEAR(underline->opacity_for_testing(), 1.f,
                kFloatComparisonTolerance);
    EXPECT_NEAR(underline->progress_for_testing(), 0.f,
                kFloatComparisonTolerance);
    EXPECT_TRUE(underline->IsShowing());
    EXPECT_FALSE(
        underline->layer()->GetCompositor()->HasAnimationObserver(underline));
  }

 private:
  base::test::ScopedFeatureList features_;
  TestFactory test_factory_;
};

class TabUnderlineViewBrowserTest : public TabUnderlineViewBrowserTestBase {
 public:
  TabUnderlineViewBrowserTest()
      : TabUnderlineViewBrowserTestBase(/*enable_gpu_rasterization=*/true) {}
  ~TabUnderlineViewBrowserTest() override = default;
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
  instance->Show(ShowOptions::ForSidePanel(*tab1));
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
  instance->Show(ShowOptions::ForSidePanel(*tab1));

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
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(incognito_browser, GURL("about:blank")));
}

IN_PROC_BROWSER_TEST_F(TabUnderlineViewBrowserTest,
                       AttachPinnedTabToNewWindow) {
  BrowserWindowInterface* browser1 = GetBrowser();
  // Set up two windows, each with one tab
  ASSERT_EQ(GetTabListInterface()->GetTabCount(), 1);
  // Second browser window, and activate it.
  BrowserWindowInterface* browser2 = CreateAdditionalBrowserWindow();
  auto* tab_list2 = TabListInterface::From(browser2);
  ASSERT_EQ(tab_list2->GetTabCount(), 1);

  // Navigate browser2's tab
  NavigateTab(*tab_list2->GetTab(0), Title2());

  // We want to open Glic on browser1, so we must ensure it is the active
  // window. Otherwise, browser2 is currently the active window.
  ASSERT_OK(ActivateWindow(browser1));

  tabs::TabHandle handle1 = GetTabListInterface()->GetTab(0)->GetHandle();
  tabs::TabHandle handle2 = tab_list2->GetTab(0)->GetHandle();
  ASSERT_TRUE(handle1.Get());
  ASSERT_TRUE(handle2.Get());

  auto& global_sharing_manager = service()->active_instance_sharing_manager();
  EXPECT_TRUE(global_sharing_manager.GetPinnedTabs().empty());

  // Toggle Glic on the first browser window to create an instance. Because this
  // browser is active, the main sharing manager will delegate to this
  // instance's sharing manager.
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  // Pin both tabs on the instance's sharing manager.
  auto& instance_sharing_manager = instance->GetSharingManagerInternal();
  instance_sharing_manager.PinTabs({handle1, handle2});
  EXPECT_TRUE(instance_sharing_manager.IsTabPinned(handle1));
  EXPECT_TRUE(instance_sharing_manager.IsTabPinned(handle2));

  // Verify the main sharing manager sees it (to show that delegation is
  // working).
  EXPECT_TRUE(global_sharing_manager.IsTabPinned(handle1));
  EXPECT_TRUE(global_sharing_manager.IsTabPinned(handle2));

  // Verify both tabs have underlines showing.
  auto* underline1 = GetUnderlineOfActiveTab(browser1);
  auto* underline2 = GetUnderlineOfActiveTab(browser2);
  ASSERT_TRUE(underline1);
  ASSERT_TRUE(underline2);
  static_cast<TesterImpl*>(underline1->tester())->WaitForAnimationStart();
  static_cast<TesterImpl*>(underline2->tester())->WaitForAnimationStart();
  EXPECT_TRUE(underline1->IsShowing());
  EXPECT_TRUE(underline2->IsShowing());

  // Simulate attachment of browser2's tab to browser1.
  tab_list2->MoveTabToWindow(handle2, browser1->GetSessionID(), 1);
  EXPECT_EQ(GetTabListInterface()->GetTabCount(), 2);

  // Check that the newly attached tab has its underline showing.
  auto* underline_attached = GetUnderlineOfTab(browser1, 1);
  ASSERT_TRUE(underline_attached);
  static_cast<TesterImpl*>(underline_attached->tester())
      ->WaitForAnimationStart();
  EXPECT_TRUE(underline_attached->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabUnderlineViewBrowserTest,
                       UnderlineDoesNotUpdateColorsWhenHidden) {
  CreateUserInitiatedTab(GURL("about:blank"));
  ASSERT_EQ(GetTabListInterface()->GetTabCount(), 2);

  auto* underline0 =
      static_cast<TestUnderlineView*>(GetUnderlineOfTab(browser(), 0));
  auto* underline1 =
      static_cast<TestUnderlineView*>(GetUnderlineOfTab(browser(), 1));
  ASSERT_TRUE(underline0);
  ASSERT_TRUE(underline1);

  EXPECT_FALSE(underline0->IsShowing());
  EXPECT_FALSE(underline1->IsShowing());
  const std::vector<SkColor> initial_colors = underline1->colors();
  EXPECT_FALSE(initial_colors.empty());

  // Changing theme and switching active tab while hidden should both
  // short-circuit without recomputing colors.
  ThemeServiceFactory::GetForProfile(browser()->GetProfile())
      ->BuildAutogeneratedThemeFromColor(SK_ColorRED);
  GetTabListInterface()->ActivateTab(
      GetTabListInterface()->GetTab(1)->GetHandle());
  EXPECT_EQ(GetTabListInterface()->GetActiveIndex(), 1);
  EXPECT_EQ(underline0->colors(), initial_colors);
  EXPECT_EQ(underline1->colors(), initial_colors);

  // Showing underline1 while tab 1 is active updates its colors to the
  // active-tab custom theme palette.
  underline1->Show();
  EXPECT_TRUE(underline1->IsShowing());
  const std::vector<SkColor> active_colors = underline1->colors();
  EXPECT_NE(active_colors, initial_colors);

  // Hide underline1, then switch active tab back to tab 0 so tab 1 becomes
  // inactive while underline1 is hidden.
  underline1->StopShowing();
  EXPECT_FALSE(underline1->IsShowing());
  GetTabListInterface()->ActivateTab(
      GetTabListInterface()->GetTab(0)->GetHandle());
  EXPECT_EQ(GetTabListInterface()->GetActiveIndex(), 0);

  // Because underline1 is hidden, OnActiveTabChanged should not have recomputed
  // its colors to the inactive-tab palette.
  EXPECT_EQ(underline1->colors(), active_colors);

  // Showing underline1 while tab 1 is inactive refreshes its colors to the
  // inactive-tab palette.
  underline1->Show();
  EXPECT_TRUE(underline1->IsShowing());
  EXPECT_NE(underline1->colors(), active_colors);
}

namespace {
class TabUnderlineViewWithoutHardwareAccelerationBrowserTest
    : public TabUnderlineViewBrowserTestBase {
 public:
  TabUnderlineViewWithoutHardwareAccelerationBrowserTest()
      : TabUnderlineViewBrowserTestBase(/*enable_gpu_rasterization=*/false) {}
  ~TabUnderlineViewWithoutHardwareAccelerationBrowserTest() override = default;
};
}  // namespace

// Ensures that when there is no hardware acceleration, the effect animation
// cycle is skipped, the view detaches as a CompositorAnimationObserver once
// the 200ms opacity ramp up completes, and ramp down still animates cleanly.
IN_PROC_BROWSER_TEST_F(TabUnderlineViewWithoutHardwareAccelerationBrowserTest,
                       BasicRampingUpAndDown) {
  ASSERT_NO_FATAL_FAILURE(VerifyRampUpWithSkippedAnimation());

  auto* underline = GetUnderlineOfActiveTab();
  ASSERT_TRUE(underline);
  TesterImpl* tester = static_cast<TesterImpl*>(underline->tester());

  ASSERT_OK(CloseGlicForTabAndWait(GetTabListInterface()->GetActiveTab()));
  tester->WaitForRampDownStarted();
  EXPECT_TRUE(
      underline->layer()->GetCompositor()->HasAnimationObserver(underline));

  // Set T0 for ramping down.
  tester->AdvanceTimeAndTickAnimation(base::TimeDelta());
  EXPECT_NEAR(underline->opacity_for_testing(), 1.f, kFloatComparisonTolerance);

  // For opacity, T=0.123s; 1-(0.123/0.2)=0.385.
  tester->AdvanceTimeAndTickAnimation(base::Seconds(0.123));
  EXPECT_NEAR(underline->opacity_for_testing(), 0.385f,
              kFloatComparisonTolerance);

  // Finish ramp down.
  tester->AdvanceTimeAndTickAnimation(base::Seconds(0.877));
  EXPECT_NEAR(underline->opacity_for_testing(), 0.f, kFloatComparisonTolerance);
  EXPECT_FALSE(underline->IsShowing());
}

namespace {
class TabUnderlineViewPrefersReducedMotionBrowserTest
    : public TabUnderlineViewBrowserTestBase {
 public:
  TabUnderlineViewPrefersReducedMotionBrowserTest()
      : TabUnderlineViewBrowserTestBase(/*enable_gpu_rasterization=*/true) {}
  ~TabUnderlineViewPrefersReducedMotionBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    TabUnderlineViewBrowserTestBase::SetUpCommandLine(command_line);
    command_line->RemoveSwitch(switches::kForcePrefersNoReducedMotion);
    command_line->AppendSwitch(switches::kForcePrefersReducedMotion);
  }
};
}  // namespace

// Ensures that when PrefersReducedMotion is true, the effect animation cycle
// is skipped and the view detaches as a CompositorAnimationObserver once the
// 200ms opacity ramp up completes.
IN_PROC_BROWSER_TEST_F(TabUnderlineViewPrefersReducedMotionBrowserTest,
                       BasicRampingUpAndDown) {
  ASSERT_TRUE(gfx::Animation::PrefersReducedMotion());
  ASSERT_NO_FATAL_FAILURE(VerifyRampUpWithSkippedAnimation());

  auto* underline = GetUnderlineOfActiveTab();
  ASSERT_TRUE(underline);
  TesterImpl* tester = static_cast<TesterImpl*>(underline->tester());

  ASSERT_OK(CloseGlicForTabAndWait(GetTabListInterface()->GetActiveTab()));
  tester->WaitForRampDownStarted();
  tester->FinishRampDown();
  EXPECT_FALSE(underline->IsShowing());
}

// Regression test for crbug.com/577211885: when a tab's underline view is
// (re)constructed while context sharing is already active, it must not attempt
// to show before it is attached to a widget, but should still end up showing.
IN_PROC_BROWSER_TEST_F(TabUnderlineViewBrowserTest,
                       AttachPinnedTabToWindowWhileSharing) {
  BrowserWindowInterface* browser1 = GetBrowser();
  BrowserWindowInterface* browser2 = CreateAdditionalBrowserWindow();
  auto* tab_list2 = TabListInterface::From(browser2);
  NavigateTab(*tab_list2->GetTab(0), Title2());
  ASSERT_OK(ActivateWindow(browser1));

  tabs::TabHandle handle1 = GetTabListInterface()->GetTab(0)->GetHandle();
  tabs::TabHandle handle2 = tab_list2->GetTab(0)->GetHandle();

  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  instance->GetSharingManagerInternal().PinTabs({handle1, handle2});
  // Turn on context sharing before the tab is moved, so the new underline view
  // observes an already-enabled indicator during construction. The service
  // only reports the indicator as enabled in live mode, so set it directly.
  instance->host().SetContextAccessIndicator(true);
  service()->SetContextAccessIndicator(true);
  ASSERT_TRUE(service()->is_context_access_indicator_enabled());

  auto* underline2 = GetUnderlineOfActiveTab(browser2);
  static_cast<TesterImpl*>(underline2->tester())->WaitForAnimationStart();
  ASSERT_TRUE(underline2->IsShowing());

  // Moving the tab reconstructs its underline view. Before the fix, the view
  // attempted to show during construction (before having a parent), which hit
  // `DumpWithoutCrashing()` in `AnimatedEffectView::Show()`. Every
  // `DumpWithoutCrashing()` call (including throttled ones) records this
  // histogram.
  base::HistogramTester histogram_tester;
  tab_list2->MoveTabToWindow(handle2, browser1->GetSessionID(), 1);
  ASSERT_EQ(GetTabListInterface()->GetTabCount(), 2);

  auto* underline_attached = GetUnderlineOfTab(browser1, 1);
  auto* tester_attached =
      static_cast<TesterImpl*>(underline_attached->tester());
  tester_attached->WaitForAnimationStart();
  EXPECT_TRUE(underline_attached->IsShowing());
  histogram_tester.ExpectTotalCount("Stability.DumpWithoutCrashingStatus", 0);
  // The initial state sync in `OnUiReady()` runs both the indicator and the
  // pinned-tab syncs, each of which requests the underline to show. Those
  // requests are batched, so the underline should be shown exactly once. A
  // count of 2 would mean a redundant show, teardown and re-show.
  EXPECT_EQ(tester_attached->animation_started_count(), 1);
}

// A show request that arrives while the underline view is not attached to a
// widget must be deferred (not call `Show()`, which would hit
// `DumpWithoutCrashing()`), and the underline should show once it is attached
// again.
IN_PROC_BROWSER_TEST_F(TabUnderlineViewBrowserTest,
                       ShowRequestWhileDetachedIsDeferred) {
  auto* underline = GetUnderlineOfActiveTab();
  TesterImpl* tester = static_cast<TesterImpl*>(underline->tester());
  ASSERT_FALSE(underline->IsShowing());

  // Detach the underline from the widget.
  views::View* parent = underline->parent();
  std::unique_ptr<TabUnderlineView> detached =
      parent->RemoveChildViewT(underline);
  ASSERT_FALSE(underline->CanShow());

  // Opening Glic pins the active tab by default, which requests the underline
  // to show while it is detached.
  base::HistogramTester histogram_tester;
  ASSERT_OK(OpenGlicForActiveTab());
  ASSERT_TRUE(service()->active_instance_sharing_manager().IsTabPinned(
      GetTabListInterface()->GetActiveTab()->GetHandle()));
  EXPECT_FALSE(underline->IsShowing());
  EXPECT_EQ(tester->animation_started_count(), 0);
  histogram_tester.ExpectTotalCount("Stability.DumpWithoutCrashingStatus", 0);

  // Re-attaching shows the underline exactly once.
  parent->AddChildView(std::move(detached));
  EXPECT_TRUE(underline->IsShowing());
  EXPECT_EQ(tester->animation_started_count(), 1);
  histogram_tester.ExpectTotalCount("Stability.DumpWithoutCrashingStatus", 0);
}

// When a showing underline is removed from its widget, it stops drawing on
// that widget's compositor, and restarts on the new compositor when it is
// attached to a widget again.
IN_PROC_BROWSER_TEST_F(TabUnderlineViewBrowserTest,
                       ReattachWhileShowingRestartsUnderline) {
  ASSERT_OK(OpenGlicForActiveTab());
  auto* underline = GetUnderlineOfActiveTab();
  TesterImpl* tester = static_cast<TesterImpl*>(underline->tester());
  // The active tab is pinned by default, so its underline shows.
  tester->WaitForAnimationStart();
  ASSERT_TRUE(underline->IsShowing());

  views::View* parent = underline->parent();
  std::unique_ptr<TabUnderlineView> detached =
      parent->RemoveChildViewT(underline);
  EXPECT_FALSE(underline->IsShowing());

  const int starts_before = tester->animation_started_count();
  parent->AddChildView(std::move(detached));
  EXPECT_TRUE(underline->IsShowing());
  EXPECT_EQ(tester->animation_started_count(), starts_before + 1);
  EXPECT_EQ(underline->layer()->GetCompositor(),
            underline->GetWidget()->GetCompositor());
}

}  // namespace glic
