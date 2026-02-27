// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include "base/numerics/ranges.h"
#include "base/strings/strcat.h"
#include "cc/test/pixel_test_utils.h"
#include "chrome/browser/glic/browser_ui/tab_underline_view.h"
#include "chrome/browser/glic/browser_ui/tab_underline_view_controller_impl.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/horizontal_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab/alert_indicator_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "gpu/config/gpu_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/base_window.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/switches.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace glic {

namespace {

using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;

static constexpr char kClickFn[] = "el => el.click()";

static constexpr float kFloatComparisonTolerance = 0.001f;

class TesterImpl : public TabUnderlineView::Tester {
 public:
  TesterImpl() = default;
  TesterImpl(const TesterImpl&) = delete;
  TesterImpl& operator=(const TesterImpl&) = delete;
  ~TesterImpl() override = default;

  // `underlineView::Tester`:
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
  raw_ptr<TabUnderlineView> underline_;
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
  TestUnderlineView(std::unique_ptr<TabUnderlineViewController> controller,
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
      std::unique_ptr<TabUnderlineViewController> controller,
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

class TabUnderlineViewUiTest : public test::InteractiveGlicTest {
 public:
  TabUnderlineViewUiTest() {
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
                                  "GlicForceNonSkSLBorder,GlicMultiInstance");
  }
  ~TabUnderlineViewUiTest() override = default;

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory(
        GetChromeTestDataDir());
    test::InteractiveGlicTest::SetUpOnMainThread();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), Title1()));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kForcePrefersNoReducedMotion);

    // This ensures that gpu rasterization (i.e hardware acceleration )is
    // available regardless of device. (This is required for underline view to
    // animate - See `AnimatedEffectView::ForceSimplifiedShader()`)
    command_line->AppendSwitch(switches::kIgnoreGpuBlocklist);
    test::InteractiveGlicTest::SetUpCommandLine(command_line);
  }

  void OpenGlicWindowAndStartSharing() {
    const DeepQuery kContextAccessIndicatorCheckBox{
        {"#contextAccessIndicator"}};
    RunTestSequence(
        // See https://crrev.com/c/6373789: the glic window is in detach mode by
        // default.
        OpenGlic(), ExecuteJsAt(test::kGlicContentsElementId,
                                kContextAccessIndicatorCheckBox, kClickFn));
  }

  void CloseGlicWindow() {
    const DeepQuery kCloseWindowButton{{"#closebn"}};
    RunTestSequence(ExecuteJsAt(test::kGlicContentsElementId,
                                kCloseWindowButton, kClickFn));
  }

  void AppendTabAndNavigate(Browser* browser, const GURL& url) {
    auto new_tab_index = browser->tab_strip_model()->active_index() + 1;
    content::TestNavigationObserver navigation_observer(url);
    navigation_observer.StartWatchingNewWebContents();
    chrome::AddTabAt(browser, url, /*index=*/new_tab_index,
                     /*foreground=*/false);
    navigation_observer.Wait();

    browser->tab_strip_model()->ActivateTabAt(new_tab_index);
  }

  GURL Title1() const { return embedded_test_server()->GetURL("/title1.html"); }

  GURL Title2() const { return embedded_test_server()->GetURL("/title2.html"); }

  TabUnderlineView* GetUnderlineOfTab(Browser* browser, int index) {
    TabStripRegionView* tab_strip_view =
        browser->window()->AsBrowserView()->tab_strip_view();
    views::View* underline =
        tab_strip_view->GetTabAnchorViewAt(index)->GetViewByElementId(
            TabUnderlineView::kGlicTabUnderlineElementId);
    CHECK(underline);
    return views::AsViewClass<TabUnderlineView>(underline);
  }

  TabUnderlineView* GetUnderlineOfActiveTab(Browser* browser = nullptr) {
    if (!browser) {
      browser = this->browser();
    }
    return GetUnderlineOfTab(browser,
                             browser->tab_strip_model()->active_index());
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

  void ActivateTabAt(int index) {
    browser()->GetTabStripModel()->ActivateTabAt(index);
  }

  glic::GlicSharingManager& sharing_manager() {
    return glic::GlicKeyedServiceFactory::GetGlicKeyedService(
               browser()->GetProfile())
        ->sharing_manager();
  }

  tabs::TabHandle TabHandleAtIndex(int index, Browser* browser = nullptr) {
    if (!browser) {
      browser = this->browser();
    }
    return browser->tab_strip_model()->GetTabAtIndex(index)->GetHandle();
  }

  void PinTabs(base::span<const tabs::TabHandle> tab_handles) {
    sharing_manager().PinTabs(tab_handles);
  }

  AlertIndicatorButton* GetAlertIndicatorButtonOfActiveTab() {
    TabStripRegionView* tab_strip_view =
        static_cast<BrowserView*>(browser()->window())->tab_strip_view();
    views::View* button =
        tab_strip_view
            ->GetTabAnchorViewAt(browser()->tab_strip_model()->active_index())
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
IN_PROC_BROWSER_TEST_F(TabUnderlineViewUiTest, SmokeTest) {
  auto* underline = GetUnderlineOfActiveTab();
  ASSERT_TRUE(underline);
  TesterImpl* tester = static_cast<TesterImpl*>(underline->tester());

  OpenGlicWindowAndStartSharing();
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

  CloseGlicWindow();
  tester->WaitForRampDownStarted();
  tester->FinishRampDown();
  EXPECT_FALSE(underline->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabUnderlineViewUiTest, ToggleSharingWithSingleTab) {
  auto* underline = GetUnderlineOfActiveTab();
  TesterImpl* tester = static_cast<TesterImpl*>(underline->tester());
  EXPECT_FALSE(underline->IsShowing());

  RunTestSequence(OpenGlic());
  EXPECT_TRUE(glic_service()->IsWindowShowing());
  // The underline should show when sharing is turned on.
  glic_service()->SetContextAccessIndicator(true);
  EXPECT_TRUE(
      glic_service()->IsContextAccessIndicatorShown(GetActiveWebContents()));

  tester->WaitForAnimationStart();
  EXPECT_TRUE(underline->IsShowing());
  tester->AdvanceTimeAndTickAnimation(base::TimeDelta());
  tester->AdvanceTimeAndTickAnimation(base::Seconds(0.3));

  // The underline should hide when sharing is turned off.
  glic_service()->SetContextAccessIndicator(false);
  ASSERT_FALSE(
      glic_service()->IsContextAccessIndicatorShown(GetActiveWebContents()));
  tester->WaitForRampDownStarted();
  tester->FinishRampDown();
  EXPECT_FALSE(underline->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabUnderlineViewUiTest,
                       SingleTabPinningWhileGlicWindowOpen) {
  RunTestSequence(OpenGlic());
  EXPECT_TRUE(glic_service()->IsWindowShowing());
  auto* underline = GetUnderlineOfActiveTab();
  TesterImpl* tester = static_cast<TesterImpl*>(underline->tester());
  EXPECT_FALSE(underline->IsShowing());

  // The underline should show when its tab is pinned.
  tabs::TabHandle tab_handle = TabHandleAtIndex(0);
  PinTabs({tab_handle});
  ASSERT_TRUE(sharing_manager().IsTabPinned(tab_handle));
  tester->WaitForAnimationStart();
  EXPECT_TRUE(underline->IsShowing());
  tester->AdvanceTimeAndTickAnimation(base::TimeDelta());
  tester->AdvanceTimeAndTickAnimation(base::Seconds(0.3));

  // The underline should hide when its tab is unpinned.
  sharing_manager().UnpinAllTabs();
  ASSERT_FALSE(sharing_manager().IsTabPinned(tab_handle));
  tester->WaitForRampDownStarted();
  tester->FinishRampDown();
  EXPECT_FALSE(underline->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabUnderlineViewUiTest,
                       SingleTabPinningWhileGlicWindowClosed) {
  EXPECT_FALSE(glic_service()->IsWindowShowing());

  // While the glic window is closed, changes to pinning have no effect on the
  // underline UI.
  auto* underline = GetUnderlineOfActiveTab();
  tabs::TabHandle tab_handle = TabHandleAtIndex(0);
  PinTabs({tab_handle});
  EXPECT_TRUE(sharing_manager().IsTabPinned(tab_handle));
  EXPECT_FALSE(underline->IsShowing());

  sharing_manager().UnpinAllTabs();
  ASSERT_FALSE(sharing_manager().IsTabPinned(tab_handle));
  EXPECT_FALSE(underline->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabUnderlineViewUiTest,
                       ToggleGlicWindowVisibilityWithPinnedTab) {
  auto* underline = GetUnderlineOfActiveTab();
  TesterImpl* tester = static_cast<TesterImpl*>(underline->tester());

  tabs::TabHandle tab_handle = TabHandleAtIndex(0);
  PinTabs({tab_handle});
  EXPECT_TRUE(sharing_manager().IsTabPinned(tab_handle));

  // The underline of a pinned tab should show when the glic window is opened.
  RunTestSequence(OpenGlic());
  tester->WaitForAnimationStart();
  EXPECT_TRUE(underline->IsShowing());
  tester->AdvanceTimeAndTickAnimation(base::TimeDelta());
  tester->AdvanceTimeAndTickAnimation(base::Seconds(0.3));

  // The underline of a pinned tab should hide when the glic window is closed.
  CloseGlicWindow();
  tester->WaitForRampDownStarted();
  tester->FinishRampDown();
  EXPECT_FALSE(underline->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabUnderlineViewUiTest, FocusedTabChange) {
  auto* underline1 = GetUnderlineOfActiveTab();
  TesterImpl* tester1 = static_cast<TesterImpl*>(underline1->tester());

  // Add second tab
  AppendTabAndNavigate(browser(), Title2());
  auto* underline2 = GetUnderlineOfActiveTab();
  TesterImpl* tester2 = static_cast<TesterImpl*>(underline2->tester());

  // The underline of the active tab should show when sharing is turned on.
  OpenGlicWindowAndStartSharing();
  tester2->WaitForAnimationStart();
  EXPECT_TRUE(underline2->IsShowing());
  tester2->AdvanceTimeAndTickAnimation(base::TimeDelta());
  tester2->AdvanceTimeAndTickAnimation(base::Seconds(0.3));

  // For tabs that are not pinned while sharing is turned on, the underline of a
  // tab that loses focus should hide and the underline of a tab that gains
  // focus should show.
  ActivateTabAt(0);
  tester1->WaitForAnimationStart();
  EXPECT_TRUE(underline1->IsShowing());
  tester1->AdvanceTimeAndTickAnimation(base::TimeDelta());
  tester1->AdvanceTimeAndTickAnimation(base::Seconds(0.3));

  tester2->WaitForRampDownStarted();
  tester2->FinishRampDown();
  EXPECT_FALSE(underline2->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabUnderlineViewUiTest,
                       FocusedTabChangeBetweenPinnedTabs) {
  auto* underline1 = GetUnderlineOfActiveTab();
  TesterImpl* tester1 = static_cast<TesterImpl*>(underline1->tester());

  AppendTabAndNavigate(browser(), Title2());
  auto* underline2 = GetUnderlineOfActiveTab();
  TesterImpl* tester2 = static_cast<TesterImpl*>(underline2->tester());

  // Pin both tabs
  PinTabs({TabHandleAtIndex(0), TabHandleAtIndex(1)});
  EXPECT_TRUE(sharing_manager().IsTabPinned(TabHandleAtIndex(0)));
  EXPECT_TRUE(sharing_manager().IsTabPinned(TabHandleAtIndex(1)));

  // Underlines of all pinned tabs should show when the glic window is opened.
  RunTestSequence(OpenGlic());
  tester1->WaitForAnimationStart();
  tester2->WaitForAnimationStart();
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
  ActivateTabAt(0);
  tester1->AdvanceTimeAndTickAnimation(base::TimeDelta());
  tester2->AdvanceTimeAndTickAnimation(base::TimeDelta());

  EXPECT_TRUE(underline1->IsShowing());
  EXPECT_TRUE(underline2->IsShowing());
  EXPECT_EQ(underline1->opacity_for_testing(), u1_opacity);
  EXPECT_EQ(underline2->opacity_for_testing(), u2_opacity);
}

IN_PROC_BROWSER_TEST_F(TabUnderlineViewUiTest, TabAlertIndicatorHidden) {
  auto* underline = GetUnderlineOfActiveTab();
  ASSERT_TRUE(underline);
  TesterImpl* tester = static_cast<TesterImpl*>(underline->tester());

  OpenGlicWindowAndStartSharing();
  tester->WaitForAnimationStart();
  EXPECT_TRUE(underline->IsShowing());

  // The shared tab should not have a visible tab alert indicator.
  EXPECT_FALSE(GetAlertIndicatorButtonOfActiveTab()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(TabUnderlineViewUiTest,
                       TabAlertIndicatorHidden_PinnedTab) {
  RunTestSequence(OpenGlic());
  EXPECT_TRUE(glic_service()->IsWindowShowing());
  auto* underline = GetUnderlineOfActiveTab();
  TesterImpl* tester = static_cast<TesterImpl*>(underline->tester());
  EXPECT_FALSE(underline->IsShowing());

  tabs::TabHandle tab_handle = TabHandleAtIndex(0);
  PinTabs({tab_handle});
  ASSERT_TRUE(sharing_manager().IsTabPinned(tab_handle));
  tester->WaitForAnimationStart();
  EXPECT_TRUE(underline->IsShowing());

  // The pinned tab should not have a visible tab alert indicator.
  EXPECT_FALSE(GetAlertIndicatorButtonOfActiveTab()->GetVisible());
}

// Ensure basic incognito window doesn't cause a crash. Simply opens an
// incognito window and navigates, test passes if it doesn't crash.
IN_PROC_BROWSER_TEST_F(TabUnderlineViewUiTest, IncognitoModeCrash) {
  Browser* incognito_browser = CreateIncognitoBrowser();

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(incognito_browser, GURL("about:blank")));
}

namespace {
class TabUnderlineViewFeatureDisabledBrowserTest
    : public TabUnderlineViewUiTest {
 public:
  TabUnderlineViewFeatureDisabledBrowserTest() {
    features_.InitAndDisableFeature(features::kGlicMultitabUnderlines);
  }
  ~TabUnderlineViewFeatureDisabledBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList features_;
};
}  // namespace

IN_PROC_BROWSER_TEST_F(TabUnderlineViewFeatureDisabledBrowserTest,
                       TabAlertIndicatorShown) {
  AlertIndicatorButton* alert_button = GetAlertIndicatorButtonOfActiveTab();
  EXPECT_FALSE(alert_button->GetVisible());

  base::RunLoop wait_for_alert_loop;
  auto callback_subscription = alert_button->AddVisibleChangedCallback(
      wait_for_alert_loop.QuitClosure());

  OpenGlicWindowAndStartSharing();
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetTabAtIndex(0);
  ASSERT_EQ(tab, sharing_manager().GetFocusedTabData().focus());

  // Wait for the view's visibility change to trigger.
  wait_for_alert_loop.Run();

  // The shared tab should have a visible tab alert indicator.
  EXPECT_TRUE(alert_button->GetVisible());
}

IN_PROC_BROWSER_TEST_F(TabUnderlineViewFeatureDisabledBrowserTest,
                       TabAlertIndicatorShown_PinnedTab) {
  AlertIndicatorButton* alert_button = GetAlertIndicatorButtonOfActiveTab();
  EXPECT_FALSE(alert_button->GetVisible());

  RunTestSequence(OpenGlic());
  EXPECT_TRUE(glic_service()->IsWindowShowing());

  base::RunLoop wait_for_alert_loop;
  auto callback_subscription = alert_button->AddVisibleChangedCallback(
      wait_for_alert_loop.QuitClosure());

  tabs::TabHandle tab_handle = TabHandleAtIndex(0);
  PinTabs({tab_handle});
  ASSERT_TRUE(sharing_manager().IsTabPinned(tab_handle));

  // Wait for the view's visibility change to trigger.
  wait_for_alert_loop.Run();

  // The pinned tab should have a visible tab alert indicator.
  EXPECT_TRUE(GetAlertIndicatorButtonOfActiveTab()->GetVisible());
}

class TabUnderlineViewMultiInstanceUiTest : public TabUnderlineViewUiTest {
 public:
  TabUnderlineViewMultiInstanceUiTest() {
    // kGlicMultiInstance, kGlicMultiTab, kGlicMultitabUnderlines are required
    // for IsMultiInstanceEnabled().
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kGlicMultiInstance,
         mojom::features::kGlicMultiTab, features::kGlicMultitabUnderlines},
        {});
  }
  ~TabUnderlineViewMultiInstanceUiTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabUnderlineViewMultiInstanceUiTest,
                       AttachPinnedTabToNewWindow) {
  // Set up two windows, each with one tab
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);
  // Second browser window; this will be active.
  Browser* browser2 = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser2, Title2()));
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);
  ASSERT_EQ(browser2->tab_strip_model()->count(), 1);

  tabs::TabHandle handle1 = TabHandleAtIndex(0);
  tabs::TabHandle handle2 = TabHandleAtIndex(0, browser2);
  ASSERT_TRUE(handle1.Get());
  ASSERT_TRUE(handle2.Get());

  // Set up glic multi-instance sharing manager.
  auto& manager = glic_service()->sharing_manager();
  EXPECT_TRUE(manager.GetPinnedTabs().empty());
  // Toggle Glic on second browser window to create an instance. Because the
  // second browser is active, the main sharing manager will delegate to this
  // instance's sharing manager.
  glic_service()->ToggleUI(browser2, false,
                           mojom::InvocationSource::kTopChromeButton);
  auto* instance = glic_service()->GetInstanceForActiveTab(browser2);
  ASSERT_TRUE(instance);

  // Pin both tabs on the instance's sharing manager.
  auto& instance_sharing_manager = instance->host().sharing_manager();
  instance_sharing_manager.PinTabs({handle1, handle2});
  EXPECT_TRUE(instance_sharing_manager.IsTabPinned(handle1));
  EXPECT_TRUE(instance_sharing_manager.IsTabPinned(handle2));

  // Verify the main sharing manager sees it (to show that delegation is
  // working).
  EXPECT_TRUE(manager.IsTabPinned(handle1));
  EXPECT_TRUE(manager.IsTabPinned(handle2));

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
  std::unique_ptr<tabs::TabModel> moved_tab =
      browser2->tab_strip_model()->DetachTabAtForInsertion(0);
  browser()->tab_strip_model()->InsertDetachedTabAt(1, std::move(moved_tab),
                                                    AddTabTypes::ADD_NONE);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);

  // Check that the newly attached tab has its underline showing.
  auto* underline_attached = GetUnderlineOfTab(browser(), 1);
  ASSERT_TRUE(underline_attached);
  static_cast<TesterImpl*>(underline_attached->tester())
      ->WaitForAnimationStart();
  EXPECT_TRUE(underline_attached->IsShowing());
}

}  // namespace glic
