// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _USE_MATH_DEFINES  // To get M_PI on Windows.

#include <math.h>

#include "base/numerics/ranges.h"
#include "base/path_service.h"
#include "cc/test/pixel_test_utils.h"
#include "chrome/browser/glic/border_view.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
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
    command_line->AppendSwitchASCII(::switches::kCSPOverride, "");
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

  glic::GlicKeyedService* glic_service(Browser* browser) {
    return glic::GlicKeyedServiceFactory::GetGlicKeyedService(
        browser->GetProfile());
  }

  void StartBorderAnimation(Browser* browser) {
    // Mimicking the user journey by clicking the button and having the WebApp
    // set the context access indicator status.
    RunTestSequence(PressButton(kGlicButtonElementId),
                    InAnyContext(WaitForShow(kGlicViewElementId)));
    // TODO(crbug.com/390233842): We should call this in the testing web app.
    glic_service(browser)->SetContextAccessIndicator(true);
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
  StartBorderAnimation(browser());
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

  StartBorderAnimation(browser());
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

  StartBorderAnimation(browser());
  EXPECT_TRUE(border->compositor_for_testing());

  // Manually stepping the animation code to mimic the behavior of the
  // compositor. As a part of crbug.com/384712084, testing via requesting
  // screenshot from the browser window was explored however, was failed due to
  // test falkiness (crbug.com/387386303).

  base::TimeTicks timestamp = base::TimeTicks::Now();

  // T=0s.
  border->OnAnimationStep(timestamp);
  EXPECT_NEAR(border->opacity_for_testing(), 0.f, kFloatComparisonTolerance);
  EXPECT_NEAR(border->emphasis_for_testing(), 0.f, kFloatComparisonTolerance);

  // T=0.333s.
  timestamp += base::Seconds(0.333);
  border->OnAnimationStep(timestamp);
  // 0.333/0.5.
  EXPECT_NEAR(border->opacity_for_testing(), 0.666, kFloatComparisonTolerance);
  // 0.333/(0.5+1.5)=0.167, 1-(1-0.167)**2=0.306
  EXPECT_NEAR(border->emphasis_for_testing(), 0.306, kFloatComparisonTolerance);

  // T=1.333s
  timestamp += base::Seconds(1);
  border->OnAnimationStep(timestamp);
  // Opacity ramp up is 0.5s.
  EXPECT_NEAR(border->opacity_for_testing(), 1.f, kFloatComparisonTolerance);
  // 1.333/(0.5+1.5)=0.667, 1-(1-0.667)**2=0.889
  EXPECT_NEAR(border->emphasis_for_testing(), 0.889, kFloatComparisonTolerance);

  // T=2.433s
  timestamp += base::Seconds(1.1);
  border->OnAnimationStep(timestamp);
  EXPECT_NEAR(border->opacity_for_testing(), 1.f, kFloatComparisonTolerance);
  // (2.433-2)/0.5=0.866, 1-(1-(1-0.866)**2)=0.018
  EXPECT_NEAR(border->emphasis_for_testing(), 0.018, kFloatComparisonTolerance);

  border->CancelAnimation();
  EXPECT_FALSE(border->compositor_for_testing());
}

// Ensures that the border animation state is reset after canceling the
// animation.
IN_PROC_BROWSER_TEST_F(GlicBorderViewUiTest, AnimationStateReset) {
  auto* border = browser()->window()->AsBrowserView()->glic_border();
  ASSERT_TRUE(border);

  StartBorderAnimation(browser());
  EXPECT_TRUE(border->compositor_for_testing());
  border->OnAnimationStep(base::TimeTicks::Now());
  border->CancelAnimation();

  EXPECT_FALSE(border->compositor_for_testing());
}

// Ensures that the border animation is restarted when tab focus changes.
IN_PROC_BROWSER_TEST_F(GlicBorderViewUiTest, FocusedTabChange) {
  auto* border = browser()->window()->AsBrowserView()->glic_border();
  ASSERT_TRUE(border);

  StartBorderAnimation(browser());
  EXPECT_TRUE(border->compositor_for_testing());

  base::TimeTicks timestamp = base::TimeTicks::Now();

  // T=0s.
  border->OnAnimationStep(timestamp);

  // T=1.333s.
  timestamp += base::Seconds(1.333);
  border->OnAnimationStep(timestamp);
  EXPECT_NEAR(border->opacity_for_testing(), 1.f, kFloatComparisonTolerance);
  EXPECT_NEAR(border->emphasis_for_testing(), 0.889, kFloatComparisonTolerance);

  // Changing the active tab.
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUINewTabURL),
                   /*index=*/-1, /*foreground=*/true);
  ASSERT_EQ(browser()->tab_strip_model()->active_index(), 1);

  // Since the active tab has changed, only the emphasis animation should
  // restart. This `OnAnimationStep()` resets the timeline of the emphasis
  // animation.
  border->OnAnimationStep(timestamp);
  // Opacity isn't reset.
  EXPECT_NEAR(border->opacity_for_testing(), 1.f, kFloatComparisonTolerance);
  // Emphasis is reset.
  EXPECT_NEAR(border->emphasis_for_testing(), 0.f, kFloatComparisonTolerance);

  // T=1.456s. For emphasis, T=0.123s.
  timestamp += base::Seconds(0.123);
  border->OnAnimationStep(timestamp);
  EXPECT_NEAR(border->opacity_for_testing(), 1.f, kFloatComparisonTolerance);
  // 0.123/(0.5+1.5)=0.062, 1-(1-0.062)**2=0.120
  EXPECT_NEAR(border->emphasis_for_testing(), 0.12, kFloatComparisonTolerance);

  // T=3.567. For emphasis, T=2.234.
  timestamp += base::Seconds(2.111);
  border->OnAnimationStep(timestamp);
  EXPECT_NEAR(border->opacity_for_testing(), 1.f, kFloatComparisonTolerance);
  // (2.234-2)/0.5=0.468, 1-(1-(1-0.468)**2)=0.283
  EXPECT_NEAR(border->emphasis_for_testing(), 0.283, kFloatComparisonTolerance);

  border->CancelAnimation();
  EXPECT_FALSE(border->compositor_for_testing());
}

IN_PROC_BROWSER_TEST_F(GlicBorderViewUiTest, FocusedWindowChange) {
  auto* border = browser()->window()->AsBrowserView()->glic_border();
  ASSERT_TRUE(border);

  StartBorderAnimation(browser());
  EXPECT_TRUE(border->compositor_for_testing());

  base::TimeTicks timestamp = base::TimeTicks::Now();

  // T=0s.
  border->OnAnimationStep(timestamp);

  // T=1.333s.
  timestamp += base::Seconds(1.333);
  border->OnAnimationStep(timestamp);
  EXPECT_NEAR(border->opacity_for_testing(), 1.f, kFloatComparisonTolerance);
  EXPECT_NEAR(border->emphasis_for_testing(), 0.889, kFloatComparisonTolerance);

  BorderView* new_border = nullptr;
  {
    SCOPED_TRACE("Wait for new window to become active");
    auto* new_browser = CreateBrowser(browser()->GetProfile());
    views::test::WaitForWidgetActive(new_browser->GetBrowserView().GetWidget(),
                                     /*active=*/true);
    new_border = new_browser->window()->AsBrowserView()->glic_border();
    ASSERT_TRUE(new_border);
    EXPECT_TRUE(new_border->compositor_for_testing());
    EXPECT_FALSE(border->compositor_for_testing());
  }

  // T=0 in the new window.
  border->OnAnimationStep(timestamp);
  EXPECT_NEAR(border->opacity_for_testing(), 0.f, kFloatComparisonTolerance);
  EXPECT_NEAR(border->emphasis_for_testing(), 0.f, kFloatComparisonTolerance);

  // T=0.123s in the new window.
  timestamp += base::Seconds(0.123);
  border->OnAnimationStep(timestamp);
  // 0.123/0.5=0.246
  EXPECT_NEAR(border->opacity_for_testing(), 0.246, kFloatComparisonTolerance);
  // 0.123/(0.5+1.5)=0.062, 1-(1-0.062)**2=0.120
  EXPECT_NEAR(border->emphasis_for_testing(), 0.12, kFloatComparisonTolerance);

  border->CancelAnimation();
  EXPECT_FALSE(border->compositor_for_testing());
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

// Ensures that in prefers-reduced-motion cases, we should immediately show the
// static border without any animations.
IN_PROC_BROWSER_TEST_F(GlicBorderViewPrefersReducedMotionUiTest,
                       PrefersReducedMotion) {
  ASSERT_TRUE(gfx::Animation::PrefersReducedMotion());
  auto* border = browser()->window()->AsBrowserView()->glic_border();
  ASSERT_TRUE(border);

  StartBorderAnimation(browser());
  EXPECT_TRUE(border->compositor_for_testing());

  base::TimeTicks timestamp = base::TimeTicks::Now();
  border->OnAnimationStep(timestamp);
  EXPECT_NEAR(border->opacity_for_testing(), 1.f, kFloatComparisonTolerance);
  EXPECT_NEAR(border->emphasis_for_testing(), 0.f, kFloatComparisonTolerance);

  timestamp += base::Seconds(2.2);
  border->OnAnimationStep(timestamp);
  EXPECT_NEAR(border->opacity_for_testing(), 1.f, kFloatComparisonTolerance);
  EXPECT_NEAR(border->emphasis_for_testing(), 0.f, kFloatComparisonTolerance);

  border->CancelAnimation();
  EXPECT_FALSE(border->compositor_for_testing());
}

}  // namespace glic
