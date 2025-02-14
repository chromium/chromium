// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include "base/numerics/ranges.h"
#include "base/path_service.h"
#include "cc/test/pixel_test_utils.h"
#include "chrome/browser/glic/glic_border_view.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/switches.h"
#include "ui/views/test/widget_activation_waiter.h"

namespace glic {

namespace {

static constexpr float kFloatComparisonTolerance = 0.001f;
static constexpr base::TimeTicks kDummyTimeStamp;

// Note: make sure to install this on the border before the animation starts.
class TesterImpl : public GlicBorderView::Tester {
 public:
  TesterImpl(GlicBorderView* border, base::TimeTicks next_time_tick)
      : border_(border) {
    next_time_tick_ = next_time_tick;
    border_->set_tester(this);
  }
  TesterImpl(const TesterImpl&) = delete;
  TesterImpl& operator=(const TesterImpl&) = delete;
  ~TesterImpl() override { border_->set_tester(nullptr); }

  // `BorderView::Tester`:
  base::TimeTicks GetTestTimestamp() override { return next_time_tick_; }
  base::TimeTicks GetTestCreationTime() override { return creation_time_; }
  void AnimationStarted() override {
    animation_started_ = true;
    wait_for_animation_started_.Quit();
  }
  void EmphasisRestarted() override {
    emphasis_restarted_ = true;
    wait_for_emphasis_restarted_.Quit();
  }

  void WaitForAnimationStart() {
    if (animation_started_) {
      return;
    }
    wait_for_animation_started_.Run();
  }

  void WaitForEmphasisRestarted() {
    if (emphasis_restarted_) {
      return;
    }
    wait_for_emphasis_restarted_.Run();
  }

  void set_next_time_tick(const base::TimeTicks& next_time_tick) {
    next_time_tick_ = next_time_tick;
  }

  void set_creation_time(const base::TimeTicks& creation_time) {
    creation_time_ = creation_time;
  }

 private:
  const raw_ptr<GlicBorderView> border_;
  base::TimeTicks next_time_tick_;
  base::TimeTicks creation_time_;

  bool animation_started_ = false;
  base::RunLoop wait_for_animation_started_;

  bool emphasis_restarted_ = false;
  base::RunLoop wait_for_emphasis_restarted_;
};

class GlicBorderViewUiTest : public InteractiveBrowserTest {
 public:
  GlicBorderViewUiTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton},
        /*disabled_features=*/{});
  }
  ~GlicBorderViewUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    ForceSigninAndModelExecutionCapability(browser()->profile());

    embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(base::DIR_ASSETS)
            .AppendASCII("gen/chrome/test/data/webui/glic/"));

    ASSERT_TRUE(embedded_test_server()->Start());

    // Need to set this here rather than in SetUpCommandLine because we need to
    // use the embedded test server to get the right URL and it's not started
    // at that time.
    auto* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(
        ::switches::kGlicGuestURL,
        embedded_test_server()->GetURL("/glic/test.html").spec());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InteractiveBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kForcePrefersNoReducedMotion);
  }

  static gfx::Rect GetContentsRectForWindow(Browser* browser) {
    auto* tab_strip_model = browser->tab_strip_model();
    EXPECT_TRUE(tab_strip_model->ContainsIndex(0));
    return tab_strip_model->GetTabAtIndex(0)->GetContents()->GetViewBounds();
  }

  static GlicButton* GetGlicButton(Browser* browser) {
    TabStripRegionView* tab_strip_view =
        browser->GetBrowserView().tab_strip_region_view();
    EXPECT_TRUE(tab_strip_view);
    return tab_strip_view->GetGlicButton();
  }

  glic::GlicKeyedService* GetGlicService(Browser* browser) {
    return glic::GlicKeyedServiceFactory::GetGlicKeyedService(
        browser->GetProfile());
  }

  void StartBorderAnimation(Browser* browser) {
    // Mimicking the user journey by clicking the button and having the WebApp
    // set the context access indicator status.
    RunTestSequence(PressButton(kGlicButtonElementId),
                    InAnyContext(WaitForShow(kGlicViewElementId)));
    // TODO(crbug.com/390233842): We should call this in the testing web app.
    GetGlicService(browser)->SetContextAccessIndicator(true);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};
}  // namespace

// Exercise that, the border is resized correctly whenever the browser's size
// changes.
IN_PROC_BROWSER_TEST_F(GlicBorderViewUiTest, BorderResize) {
  // TODO(crbug.com/385828490): We should exercise the proper closing flow.
  // Currently the BookmarkModel has a dangling observer during destruction, if
  // the glic UI is toggled.
  auto* border = browser()->window()->AsBrowserView()->glic_border();
  ASSERT_TRUE(border);
  TesterImpl tester(border, base::TimeTicks::Now());
  StartBorderAnimation(browser());
  tester.WaitForAnimationStart();
  auto* contents_web_view = browser()->GetBrowserView().contents_web_view();
  EXPECT_EQ(border->GetVisibleBounds(), contents_web_view->GetVisibleBounds());

  // Note: there is a minimal size that the desktop window can be. It seems to
  // be around 500px by 500px.
  const gfx::Size new_size(600, 600);
  auto* browser_window = browser()->window();
  const gfx::Rect new_bounds(browser_window->GetBounds().origin(), new_size);
  EXPECT_NE(browser_window->GetBounds(), new_bounds);

  {
    SCOPED_TRACE("resizing");
    browser_window->SetBounds(new_bounds);
    content::RunAllPendingInMessageLoop();
  }

  // Resized correctly.
  EXPECT_EQ(browser_window->GetBounds(), new_bounds);
  EXPECT_EQ(border->GetVisibleBounds(), contents_web_view->GetVisibleBounds());
}

// Regression test for https://crbug.com/387458471: The border shouldn't be
// visible before StartAnimation is called, and shouldn't be visible after
// CancelAnimation is called.
IN_PROC_BROWSER_TEST_F(GlicBorderViewUiTest, Visibility) {
  auto* border = browser()->window()->AsBrowserView()->glic_border();
  ASSERT_TRUE(border);
  EXPECT_FALSE(border->GetVisible());

  TesterImpl tester(border, base::TimeTicks::Now());
  StartBorderAnimation(browser());
  tester.WaitForAnimationStart();
  EXPECT_TRUE(border->compositor_for_testing());
  EXPECT_TRUE(border->GetVisible());
  border->CancelAnimation();
  EXPECT_FALSE(border->GetVisible());
}

// Exercise the default user journey: toggles the border animation and wait for
// it to finish.
IN_PROC_BROWSER_TEST_F(GlicBorderViewUiTest, SmokeTest) {
  auto* border = browser()->window()->AsBrowserView()->glic_border();
  ASSERT_TRUE(border);
  base::TimeTicks timestamp = base::TimeTicks::Now();
  TesterImpl tester(border, timestamp);

  StartBorderAnimation(browser());
  tester.WaitForAnimationStart();
  EXPECT_TRUE(border->compositor_for_testing());

  // Manually stepping the animation code to mimic the behavior of the
  // compositor. As a part of crbug.com/384712084, testing via requesting
  // screenshot from the browser window was explored however, was failed due to
  // test falkiness (crbug.com/387386303).

  // T=0s.
  border->OnAnimationStep(kDummyTimeStamp);
  EXPECT_NEAR(border->opacity_for_testing(), 0.f, kFloatComparisonTolerance);
  EXPECT_NEAR(border->emphasis_for_testing(), 0.f, kFloatComparisonTolerance);

  // T=0.333s.
  timestamp += base::Seconds(0.333);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  // 0.333/0.5.
  EXPECT_NEAR(border->opacity_for_testing(), 0.666, kFloatComparisonTolerance);
  // 0.333/0.5=0.666, 1-(1-0.666)**2~=0.888
  EXPECT_NEAR(border->emphasis_for_testing(), 0.888, kFloatComparisonTolerance);

  // T=1.333s
  timestamp += base::Seconds(1);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  // Opacity ramp up is 0.5s.
  EXPECT_NEAR(border->opacity_for_testing(), 1.f, kFloatComparisonTolerance);
  // clamped 1.333/0.5 -> 1.0, 1-(1-1.0.667)**2=1.0
  EXPECT_NEAR(border->emphasis_for_testing(), 1.f, kFloatComparisonTolerance);

  // T=2.433s
  timestamp += base::Seconds(1.1);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  EXPECT_NEAR(border->opacity_for_testing(), 1.f, kFloatComparisonTolerance);
  // (2.433-2)/1.0=0.433
  EXPECT_NEAR(
      border->emphasis_for_testing(),
      1.f - gfx::Tween::CalculateValue(gfx::Tween::Type::EASE_IN_OUT_2, 0.433),
      kFloatComparisonTolerance);

  border->CancelAnimation();
  EXPECT_FALSE(border->compositor_for_testing());
}

// Ensures that the border animation state is reset after canceling the
// animation.
IN_PROC_BROWSER_TEST_F(GlicBorderViewUiTest, AnimationStateReset) {
  auto* border = browser()->window()->AsBrowserView()->glic_border();
  ASSERT_TRUE(border);

  TesterImpl tester(border, base::TimeTicks::Now());
  StartBorderAnimation(browser());
  tester.WaitForAnimationStart();
  EXPECT_TRUE(border->compositor_for_testing());
  border->OnAnimationStep(base::TimeTicks::Now());
  border->CancelAnimation();

  EXPECT_FALSE(border->compositor_for_testing());
  EXPECT_FALSE(border->opacity_for_testing());
  EXPECT_FALSE(border->emphasis_for_testing());
}

// Ensures that the border animation is restarted when tab focus changes.
IN_PROC_BROWSER_TEST_F(GlicBorderViewUiTest, FocusedTabChange) {
  auto* border = browser()->window()->AsBrowserView()->glic_border();
  ASSERT_TRUE(border);
  base::TimeTicks timestamp = base::TimeTicks::Now();
  TesterImpl tester(border, timestamp);

  StartBorderAnimation(browser());
  tester.WaitForAnimationStart();
  EXPECT_TRUE(border->compositor_for_testing());

  // T=0s.
  border->OnAnimationStep(kDummyTimeStamp);

  // T=1.333s.
  timestamp += base::Seconds(1.333);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  EXPECT_NEAR(border->opacity_for_testing(), 1.f, kFloatComparisonTolerance);
  EXPECT_NEAR(border->emphasis_for_testing(), 1.f, kFloatComparisonTolerance);

  // Changing the active tab.
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUINewTabURL),
                   /*index=*/-1, /*foreground=*/true);
  ASSERT_EQ(browser()->tab_strip_model()->active_index(), 1);
  tester.WaitForEmphasisRestarted();

  // Since the active tab has changed, only the emphasis animation should
  // restart. This `OnAnimationStep()` resets the timeline of the emphasis
  // animation.
  border->OnAnimationStep(kDummyTimeStamp);
  // Opacity isn't reset.
  EXPECT_NEAR(border->opacity_for_testing(), 1.f, kFloatComparisonTolerance);
  // Emphasis is reset.
  EXPECT_NEAR(border->emphasis_for_testing(), 0.f, kFloatComparisonTolerance);

  // T=1.456s. For emphasis, T=0.123s.
  timestamp += base::Seconds(0.123);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  EXPECT_NEAR(border->opacity_for_testing(), 1.f, kFloatComparisonTolerance);
  // 0.123/0.5=0.246, 1-(1-0.246)**2=0.431
  EXPECT_NEAR(border->emphasis_for_testing(), 0.431, kFloatComparisonTolerance);

  // T=3.567. For emphasis, T=2.234.
  timestamp += base::Seconds(2.111);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  EXPECT_NEAR(border->opacity_for_testing(), 1.f, kFloatComparisonTolerance);
  // (2.234-2)/1.0=0.234
  EXPECT_NEAR(
      border->emphasis_for_testing(),
      1.f - gfx::Tween::CalculateValue(gfx::Tween::Type::EASE_IN_OUT_2, 0.234),
      kFloatComparisonTolerance);

  border->CancelAnimation();
  EXPECT_FALSE(border->compositor_for_testing());
}

IN_PROC_BROWSER_TEST_F(GlicBorderViewUiTest, FocusedWindowChange) {
  auto* border = browser()->window()->AsBrowserView()->glic_border();
  ASSERT_TRUE(border);
  base::TimeTicks timestamp = base::TimeTicks::Now();
  auto tester = std::make_unique<TesterImpl>(border, timestamp);

  StartBorderAnimation(browser());
  tester->WaitForAnimationStart();
  EXPECT_TRUE(border->compositor_for_testing());

  // T=0s.
  border->OnAnimationStep(kDummyTimeStamp);

  // T=1.333s.
  timestamp += base::Seconds(1.333);
  tester->set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  EXPECT_NEAR(border->opacity_for_testing(), 1.f, kFloatComparisonTolerance);
  EXPECT_NEAR(border->emphasis_for_testing(), 1.f, kFloatComparisonTolerance);

  GlicBorderView* new_border = nullptr;
  std::unique_ptr<TesterImpl> new_tester;
  base::TimeTicks new_timestamp;
  {
    SCOPED_TRACE("Wait for new window to become active");
    auto* new_browser = CreateBrowser(browser()->GetProfile());
    new_border = new_browser->window()->AsBrowserView()->glic_border();
    new_timestamp = base::TimeTicks::Now();
    new_tester = std::make_unique<TesterImpl>(new_border, new_timestamp);
    views::test::WaitForWidgetActive(new_browser->GetBrowserView().GetWidget(),
                                     /*active=*/true);
    new_tester->WaitForAnimationStart();
  }
  ASSERT_TRUE(new_border);
  EXPECT_TRUE(new_border->compositor_for_testing());
  // The first `OnAnimationStep()` on the defocused border starts the ramp
  // down sequence. After 0.5s, the ramp down has finished.
  border->OnAnimationStep(kDummyTimeStamp);
  timestamp += base::Seconds(0.5);
  tester->set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  EXPECT_FALSE(border->compositor_for_testing());

  // T=0 in the new window.
  new_border->OnAnimationStep(kDummyTimeStamp);
  EXPECT_NEAR(new_border->opacity_for_testing(), 0.f,
              kFloatComparisonTolerance);
  EXPECT_NEAR(new_border->emphasis_for_testing(), 0.f,
              kFloatComparisonTolerance);

  // T=0.123s in the new window.
  new_timestamp += base::Seconds(0.123);
  new_tester->set_next_time_tick(new_timestamp);
  new_border->OnAnimationStep(kDummyTimeStamp);
  // 0.123/0.5=0.246
  EXPECT_NEAR(new_border->opacity_for_testing(), 0.246,
              kFloatComparisonTolerance);
  // 0.123/0.5=0.246, 1-(1-0.246)**2=0.120
  EXPECT_NEAR(new_border->emphasis_for_testing(), 0.431,
              kFloatComparisonTolerance);

  new_border->CancelAnimation();
  EXPECT_FALSE(new_border->compositor_for_testing());
}

// Ensures that the border fades out before disappearing entirely during
// emphasis ramp up.
IN_PROC_BROWSER_TEST_F(GlicBorderViewUiTest, RampingDownDuringEmphasisRampUp) {
  auto* border = browser()->window()->AsBrowserView()->glic_border();
  ASSERT_TRUE(border);
  base::TimeTicks timestamp = base::TimeTicks::Now();
  TesterImpl tester(border, timestamp);

  StartBorderAnimation(browser());
  tester.WaitForAnimationStart();
  EXPECT_TRUE(border->compositor_for_testing());

  // T=0s.
  border->OnAnimationStep(kDummyTimeStamp);

  // T=0.333s.
  timestamp += base::Seconds(0.333);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  EXPECT_NEAR(border->opacity_for_testing(), 0.666, kFloatComparisonTolerance);
  // 0.333/0.5=0.666, 1-(1-0.333)**2=0.888
  EXPECT_NEAR(border->emphasis_for_testing(), 0.888, kFloatComparisonTolerance);

  // Closing the glic window must start the ramping down process.
  GetGlicService(browser())->ClosePanel();

  // Calling `OnAnimationStep()` will set the start time of ramping down.
  // T = 0.333s; for opacity, T=0s.
  border->OnAnimationStep(kDummyTimeStamp);
  // Opacity must start from its most recent value and decrease.
  EXPECT_NEAR(border->opacity_for_testing(), 0.666, kFloatComparisonTolerance);
  // Emphasis should remain as is.
  EXPECT_NEAR(border->emphasis_for_testing(), 0.888, kFloatComparisonTolerance);

  // T=0.456s. For opacity, T=0.123s.
  timestamp += base::Seconds(0.123);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  // 0.666-(0.123/0.2) = 0.051.
  EXPECT_NEAR(border->opacity_for_testing(), 0.051, kFloatComparisonTolerance);
  // 0.456/0.5=0.912, 1-(1-0.912)**2=0.926
  EXPECT_NEAR(border->emphasis_for_testing(), 0.992, kFloatComparisonTolerance);

  // T=0.526s. For opacity, T=0.193s.
  timestamp += base::Seconds(0.07);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  // clamp 0.666-(0.193/0.2) = 0.0
  EXPECT_NEAR(border->opacity_for_testing(), 0.f, kFloatComparisonTolerance);
  // 0.52/0.5 -> 1, however since CancelAnimation has been invoked (this
  // happens when the opacity ramp down is done in order to clean up), emphasis
  // is reset to zero and the compositor is reset.
  EXPECT_NEAR(border->emphasis_for_testing(), 0.f, kFloatComparisonTolerance);
  EXPECT_FALSE(border->compositor_for_testing());
}

// Ensures that the border fades out before disappearing entirely during opacity
// ramp up.
IN_PROC_BROWSER_TEST_F(GlicBorderViewUiTest, RampingDownDuringOpacityRampUp) {
  auto* border = browser()->window()->AsBrowserView()->glic_border();
  ASSERT_TRUE(border);
  base::TimeTicks timestamp = base::TimeTicks::Now();
  TesterImpl tester(border, timestamp);

  StartBorderAnimation(browser());
  tester.WaitForAnimationStart();
  EXPECT_TRUE(border->compositor_for_testing());

  // T=0s.
  border->OnAnimationStep(kDummyTimeStamp);

  // T=0.3s.
  timestamp += base::Seconds(0.3);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  // (0.3/0.5)=0.6
  EXPECT_NEAR(border->opacity_for_testing(), 0.6, kFloatComparisonTolerance);
  // 0.3/0.5=0.6, 1-(1-0.6)**2=0.84
  EXPECT_NEAR(border->emphasis_for_testing(), 0.84, kFloatComparisonTolerance);

  // Closing the glic window must start the ramping down process.
  GetGlicService(browser())->ClosePanel();

  // Calling `OnAnimationStep()` will set the start time of ramping down.
  // T = 0.3s; for opacity, T=0s.
  border->OnAnimationStep(kDummyTimeStamp);
  // Opacity must start from its most recent value and decrease.
  EXPECT_NEAR(border->opacity_for_testing(), 0.6, kFloatComparisonTolerance);
  // Emphasis should remain as is.
  EXPECT_NEAR(border->emphasis_for_testing(), 0.84, kFloatComparisonTolerance);

  // T=0.406s. For opacity, T=0.106s.
  timestamp += base::Seconds(0.106);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  // 0.6-(0.106/0.2)=0.07
  EXPECT_NEAR(border->opacity_for_testing(), 0.07, kFloatComparisonTolerance);
  // 0.406/0.5=0.812, 1-(1-0.812)**2=0.965
  EXPECT_NEAR(border->emphasis_for_testing(), 0.965, kFloatComparisonTolerance);

  // T=0.45s. For opacity, T=0.15s.
  timestamp += base::Seconds(0.044);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  // clamp 0.6-(0.15/0.2) -> 0
  EXPECT_NEAR(border->opacity_for_testing(), 0.f, kFloatComparisonTolerance);
  // 0.45/0.5=0.9, 1-(1-0.9)**2=0.99.
  // However since CancelAnimation has been invoked (this happens when the
  // opacity ramp down is done in order to clean up), emphasis is reset to
  // zero and the compositor is reset.
  EXPECT_NEAR(border->emphasis_for_testing(), 0.f, kFloatComparisonTolerance);
  EXPECT_FALSE(border->compositor_for_testing());
}

// Ensures that the border fades out before disappearing entirely during stable
// state.
IN_PROC_BROWSER_TEST_F(GlicBorderViewUiTest, RampingDownDuringStableState) {
  auto* border = browser()->window()->AsBrowserView()->glic_border();
  ASSERT_TRUE(border);
  base::TimeTicks timestamp = base::TimeTicks::Now();
  TesterImpl tester(border, timestamp);

  StartBorderAnimation(browser());
  tester.WaitForAnimationStart();
  EXPECT_TRUE(border->compositor_for_testing());

  // T=0s.
  border->OnAnimationStep(kDummyTimeStamp);

  // T=5s.
  timestamp += base::Seconds(5);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  EXPECT_NEAR(border->opacity_for_testing(), 1.f, kFloatComparisonTolerance);
  EXPECT_NEAR(border->emphasis_for_testing(), 0.f, kFloatComparisonTolerance);

  // Closing the glic window must start the ramping down process.
  GetGlicService(browser())->ClosePanel();

  // Calling `OnAnimationStep()` will set the start time of ramping down.
  // T = 5s; for opacity, T=0s.
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  // Opacity must start from its most recent value and decrease.
  EXPECT_NEAR(border->opacity_for_testing(), 1.f, kFloatComparisonTolerance);
  // Emphasis should remain as is.
  EXPECT_NEAR(border->emphasis_for_testing(), 0.f, kFloatComparisonTolerance);

  // T=5.05s. For opacity, T=0.05s.
  timestamp += base::Seconds(0.05);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  // 1-(0.05/0.2)=0.75
  EXPECT_NEAR(border->opacity_for_testing(), 0.75, kFloatComparisonTolerance);
  EXPECT_NEAR(border->emphasis_for_testing(), 0.f, kFloatComparisonTolerance);

  // T=5.12s. For opacity, T=0.12s.
  timestamp += base::Seconds(0.07);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  // 1-(0.12/0.2)=0.4
  EXPECT_NEAR(border->opacity_for_testing(), 0.4, kFloatComparisonTolerance);
  EXPECT_NEAR(border->emphasis_for_testing(), 0.0f, kFloatComparisonTolerance);

  timestamp += base::Seconds(5);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  EXPECT_FALSE(border->compositor_for_testing());
}

IN_PROC_BROWSER_TEST_F(GlicBorderViewUiTest, EnsureTimeWraps) {
  auto* border = browser()->window()->AsBrowserView()->glic_border();
  ASSERT_TRUE(border);

  base::TimeTicks timestamp = base::TimeTicks::Now();
  TesterImpl tester(border, timestamp);
  StartBorderAnimation(browser());
  tester.WaitForAnimationStart();
  tester.set_creation_time(timestamp);
  float seconds = border->GetEffectTimeForTesting();

  timestamp += base::Days(0.5);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  float seconds_half_day = border->GetEffectTimeForTesting();

  // Should not have wrapped.
  EXPECT_LT(seconds, seconds_half_day);

  timestamp += base::Days(0.5);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);

  // Now that more than a day has passed, we should have wrapped (and so the
  // ms since creation should be lower than at the half-day mark).
  EXPECT_GT(seconds_half_day, border->GetEffectTimeForTesting());
}

namespace {
class GlicBorderViewFeatureDisabledBrowserTest : public GlicBorderViewUiTest {
 public:
  GlicBorderViewFeatureDisabledBrowserTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature(features::kGlic);
  }
  ~GlicBorderViewFeatureDisabledBrowserTest() override = default;
};
}  // namespace

// Regression test for https://crbug.com/387458471: The border is not
// initialized if the feature is disabled.
IN_PROC_BROWSER_TEST_F(GlicBorderViewFeatureDisabledBrowserTest, NoBorder) {
  auto* border = browser()->window()->AsBrowserView()->glic_border();
  EXPECT_FALSE(border);
}

namespace {
class GlicBorderViewPrefersReducedMotionUiTest : public GlicBorderViewUiTest {
 public:
  GlicBorderViewPrefersReducedMotionUiTest() = default;
  ~GlicBorderViewPrefersReducedMotionUiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicBorderViewUiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kForcePrefersReducedMotion);
  }
};
}  // namespace

// Ensures that when PrefersReducedMotion is true, the emphasis animation is
// skipped and we just show an opacity ramp up and ramp down animation.
// Note: Ramp up and ramp down duration in PrefersReducedMotion is 200ms..
IN_PROC_BROWSER_TEST_F(GlicBorderViewPrefersReducedMotionUiTest,
                       BasicRampingUpAndDown) {
  ASSERT_TRUE(gfx::Animation::PrefersReducedMotion());
  auto* border = browser()->window()->AsBrowserView()->glic_border();
  ASSERT_TRUE(border);
  base::TimeTicks timestamp = base::TimeTicks::Now();
  TesterImpl tester(border, timestamp);

  StartBorderAnimation(browser());
  tester.WaitForAnimationStart();
  EXPECT_TRUE(border->compositor_for_testing());

  // ---- Ramping up ----
  // T=0s.
  border->OnAnimationStep(kDummyTimeStamp);

  // T=0.123s.
  timestamp += base::Seconds(0.123);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  // Opacity ramp up is 0.2; 0.123/0.2=0.615
  EXPECT_NEAR(border->opacity_for_testing(), 0.615, kFloatComparisonTolerance);
  EXPECT_NEAR(border->emphasis_for_testing(), 0.f, kFloatComparisonTolerance);

  // T=0.146s.
  timestamp += base::Seconds(0.023);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  // 0.146/0.2=0.73
  EXPECT_NEAR(border->opacity_for_testing(), 0.73, kFloatComparisonTolerance);
  EXPECT_NEAR(border->emphasis_for_testing(), 0.f, kFloatComparisonTolerance);

  // T=1s.
  timestamp += base::Seconds(0.854);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  EXPECT_NEAR(border->opacity_for_testing(), 1.f, kFloatComparisonTolerance);
  EXPECT_NEAR(border->emphasis_for_testing(), 0.f, kFloatComparisonTolerance);

  // ---- Ramping down ----
  // Closing the glic window must start the ramping down process.
  GetGlicService(browser())->ClosePanel();

  // T=1s; For opacity T=0s.
  // Calling `OnAnimationStep()` will set the start time of ramping down.
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  EXPECT_NEAR(border->opacity_for_testing(), 1.f, kFloatComparisonTolerance);
  EXPECT_NEAR(border->emphasis_for_testing(), 0.f, kFloatComparisonTolerance);

  // T=1.123s. For opacity, T=0.123s.
  timestamp += base::Seconds(0.123);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  // 1-(0.123/0.2)=0.385
  EXPECT_NEAR(border->opacity_for_testing(), 0.385, kFloatComparisonTolerance);
  EXPECT_NEAR(border->emphasis_for_testing(), 0.f, kFloatComparisonTolerance);

  // T=1.134s. For opacity, T=0.134s.
  timestamp += base::Seconds(0.011);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  // 1-(0.134/0.2)=0.33
  EXPECT_NEAR(border->opacity_for_testing(), 0.33, kFloatComparisonTolerance);
  EXPECT_NEAR(border->emphasis_for_testing(), 0.f, kFloatComparisonTolerance);

  // T=2s. For opacity, T=1s.
  timestamp += base::Seconds(0.866);
  tester.set_next_time_tick(timestamp);
  border->OnAnimationStep(kDummyTimeStamp);
  EXPECT_NEAR(border->opacity_for_testing(), 0.f, kFloatComparisonTolerance);
  EXPECT_NEAR(border->emphasis_for_testing(), 0.f, kFloatComparisonTolerance);
  EXPECT_FALSE(border->compositor_for_testing());
}

}  // namespace glic
