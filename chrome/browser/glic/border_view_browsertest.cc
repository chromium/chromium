// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _USE_MATH_DEFINES  // To get M_PI on Windows.

#include "chrome/browser/glic/border_view.h"

#include <math.h>

#include "base/path_service.h"
#include "cc/test/pixel_test_utils.h"
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
#include "ui/views/test/widget_activation_waiter.h"

namespace glic {

namespace {

static const SkColor kBlack = SkColors::kBlack.toSkColor();
// The border color's alpha could make the RGB value not exact (e.g, 127 vs
// 128). We allow one-off in each channel for comparison.
static constexpr int kPxComparisonTolerance = 3;

class BorderViewBrowserTest : public InteractiveBrowserTest {
 public:
  BorderViewBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton},
        /*disabled_features=*/{});
  }
  ~BorderViewBrowserTest() override = default;

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

  SkBitmap PaintBorder(BorderView* border) {
    gfx::Canvas canvas(GetContentsRectForWindow(browser()).size(),
                       /*image_scale=*/1.0f,
                       /*is_opaque=*/true);
    canvas.DrawColor(kBlack);
    border->OnPaint(&canvas);
    return canvas.GetBitmap();
  }

  SkColor BorderColor() {
    return browser()->GetBrowserView().GetColorProvider()->GetColor(
        ui::kColorSysPrimary);
  }

  static SkBitmap ConstructExpectedBitmap(const gfx::Size& size,
                                          SkColor border_color,
                                          SkColor center_color,
                                          float border_width,
                                          float alpha) {
    SkBitmap bitmap;
    SkImageInfo info =
        SkImageInfo::Make(size.width(), size.height(), kRGBA_8888_SkColorType,
                          kUnpremul_SkAlphaType);
    bitmap.allocPixels(info);
    bitmap.eraseColor(center_color);
    SkCanvas canvas(bitmap, SkSurfaceProps{});
    SkPaint border;
    border.setColor(border_color);
    border.setStyle(SkPaint::Style::kStroke_Style);
    border.setStrokeWidth(border_width);
    border.setAlphaf(alpha);
    canvas.drawRect(SkRect::MakeXYWH(0, 0, size.width(), size.height()),
                    border);
    return bitmap;
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

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};
}  // namespace

// Exercise that, the border is resized correctly whenever the browser's size
// changes.
IN_PROC_BROWSER_TEST_F(BorderViewBrowserTest, BorderResize) {
  // TODO(crbug.com/385828490): We should exercise the proper closing flow.
  // Currently the BookmarkModel has a dangling observer during destruction, if
  // the glic UI is toggled.
  auto* border = browser()->window()->AsBrowserView()->glic_border();
  ASSERT_TRUE(border);
  border->StartAnimation();
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
IN_PROC_BROWSER_TEST_F(BorderViewBrowserTest, Visibility) {
  auto* border = browser()->window()->AsBrowserView()->glic_border();
  ASSERT_TRUE(border);
  EXPECT_FALSE(border->GetVisible());
  border->StartAnimation();
  EXPECT_TRUE(border->GetVisible());
  border->CancelAnimation();
  EXPECT_FALSE(border->GetVisible());
}

// Ensures that the border width is correct in various timestamps during the
// animation.
IN_PROC_BROWSER_TEST_F(BorderViewBrowserTest, AnimationSteps) {
  auto* border = browser()->window()->AsBrowserView()->glic_border();
  ASSERT_TRUE(border);
  gfx::Rect capture_rect = GetContentsRectForWindow(browser());

  border->StartAnimation();

  // Manually stepping the animation code to mimic the behavior of the
  // compositor. As a part of crbug.com/384712084, testing via requesting
  // screenshot from the browser window was explored however, was failed due to
  // test falkiness (crbug.com/387386303).

  base::TimeTicks timestamp = base::TimeTicks::Now();

  // Note: the following is based on having animation duration = 2 seconds.
  // timestamp = 0 (now).
  {
    border->OnAnimationStep(timestamp);
    SkBitmap actual_bitmap = PaintBorder(border);

    SkBitmap expected_bitmap = ConstructExpectedBitmap(
        capture_rect.size(),
        /*border_color=*/BorderColor(),
        /*center_color=*/kBlack, /*border_width=*/2.f, /*alpha=*/0.f);

    EXPECT_TRUE(cc::MatchesBitmap(
        actual_bitmap, expected_bitmap,
        cc::ManhattanDistancePixelComparator(kPxComparisonTolerance)));
  }

  // timestamp = 0.5 seconds.
  {
    timestamp += base::Seconds(0.5);
    border->OnAnimationStep(timestamp);
    SkBitmap actual_bitmap = PaintBorder(border);

    // The sin input is calculated as:
    // (since_first_frame / kDuration) * (PI * 2)
    // At timestamp = 0.5 and with kAnimationDuration = 2,
    // we have: (0.5 / 2) * (PI * 2) = 0.125 * M_PI.
    float progress = sin(0.125 * M_PI);

    // The border width is calculated as:
    // `kBorderWidthMin` + ((`kBorderWidthMax` - `kBorderWidthMin`) *
    // `progress`).
    float border_width = 2 + (8 * progress);
    SkBitmap expected_bitmap = ConstructExpectedBitmap(
        capture_rect.size(),
        /*border_color=*/BorderColor(),
        /*center_color=*/kBlack, /*border_width=*/border_width,
        /*alpha=*/progress);

    EXPECT_TRUE(cc::MatchesBitmap(
        actual_bitmap, expected_bitmap,
        cc::ManhattanDistancePixelComparator(kPxComparisonTolerance)));
  }

  // timestamp = 2 seconds.
  {
    timestamp += base::Seconds(1.5);
    border->OnAnimationStep(timestamp);
    SkBitmap actual_bitmap = PaintBorder(border);

    SkBitmap expected_bitmap =
        ConstructExpectedBitmap(capture_rect.size(),
                                /*border_color=*/BorderColor(),
                                /*center_color=*/kBlack, /*border_width=*/10.f,
                                /*alpha=*/1.f);

    EXPECT_TRUE(cc::MatchesBitmap(
        actual_bitmap, expected_bitmap,
        cc::ManhattanDistancePixelComparator(kPxComparisonTolerance)));
  }

  // timestamp = 4 seconds; stable state.
  {
    timestamp += base::Seconds(2);
    border->OnAnimationStep(timestamp);
    SkBitmap actual_bitmap = PaintBorder(border);

    SkBitmap expected_bitmap = ConstructExpectedBitmap(
        capture_rect.size(),
        /*border_color=*/BorderColor(),
        /*center_color=*/kBlack, /*border_width=*/10.f, /*alpha=*/1.f);

    EXPECT_TRUE(cc::MatchesBitmap(
        actual_bitmap, expected_bitmap,
        cc::ManhattanDistancePixelComparator(kPxComparisonTolerance)));
  }

  border->CancelAnimation();
}

// Ensures that the border animation state is reset after canceling the
// animation.
IN_PROC_BROWSER_TEST_F(BorderViewBrowserTest, AnimationStateReset) {
  auto* border = browser()->window()->AsBrowserView()->glic_border();
  ASSERT_TRUE(border);

  border->StartAnimation();
  border->OnAnimationStep(base::TimeTicks::Now());
  border->CancelAnimation();

  EXPECT_FALSE(border->compositor_for_testing());
}

// Ensures that the border animation is restarted when tab focus changes.
IN_PROC_BROWSER_TEST_F(BorderViewBrowserTest, FocusedTabChange) {
  auto* border = browser()->window()->AsBrowserView()->glic_border();
  ASSERT_TRUE(border);
  gfx::Rect capture_rect = GetContentsRectForWindow(browser());

  // Mimicking the user journey by clicking the button and having the WebApp set
  // the context access indicator status.
  auto* const glic_keyed_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(
          browser()->GetProfile());
  GetGlicButton(browser())->LaunchUI();
  glic_keyed_service->SetContextAccessIndicator(true);
  EXPECT_TRUE(border->compositor_for_testing());

  base::TimeTicks timestamp = base::TimeTicks::Now();

  // Note: the following is based on having animation duration = 2 seconds.
  // timestamp = 0 (now).
  {
    border->OnAnimationStep(timestamp);
    SkBitmap actual_bitmap = PaintBorder(border);

    SkBitmap expected_bitmap = ConstructExpectedBitmap(
        capture_rect.size(),
        /*border_color=*/BorderColor(),
        /*center_color=*/kBlack, /*border_width=*/2.f, /*alpha=*/0.f);

    EXPECT_TRUE(cc::MatchesBitmap(
        actual_bitmap, expected_bitmap,
        cc::ManhattanDistancePixelComparator(kPxComparisonTolerance)));
  }

  // timestamp = 0.5 seconds.
  {
    timestamp += base::Seconds(0.5);
    border->OnAnimationStep(timestamp);
    SkBitmap actual_bitmap = PaintBorder(border);

    float progress = sin(0.125 * M_PI);
    float border_width = 2 + (8 * progress);
    SkBitmap expected_bitmap = ConstructExpectedBitmap(
        capture_rect.size(),
        /*border_color=*/BorderColor(),
        /*center_color=*/kBlack, /*border_width=*/border_width,
        /*alpha=*/progress);

    EXPECT_TRUE(cc::MatchesBitmap(
        actual_bitmap, expected_bitmap,
        cc::ManhattanDistancePixelComparator(kPxComparisonTolerance)));
  }

  // Changing the active tab.
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUINewTabURL),
                   /*index=*/-1, /*foreground=*/true);
  ASSERT_EQ(browser()->tab_strip_model()->active_index(), 1);

  // Since the active tab has changed, the animation should start from the
  // beginning.
  // timestamp = 6 seconds; second 0 in the current animation.
  {
    timestamp += base::Seconds(5.5);
    border->OnAnimationStep(timestamp);
    SkBitmap actual_bitmap = PaintBorder(border);

    SkBitmap expected_bitmap =
        ConstructExpectedBitmap(capture_rect.size(),
                                /*border_color=*/BorderColor(),
                                /*center_color=*/kBlack, /*border_width=*/2.f,
                                /*alpha=*/0.f);

    EXPECT_TRUE(cc::MatchesBitmap(
        actual_bitmap, expected_bitmap,
        cc::ManhattanDistancePixelComparator(kPxComparisonTolerance)));
  }

  // timestamp = 6.5 seconds; second 0.5 in the current animation.
  {
    timestamp += base::Seconds(0.5);
    border->OnAnimationStep(timestamp);
    SkBitmap actual_bitmap = PaintBorder(border);

    float progress = sin(0.125 * M_PI);
    float border_width = 2 + (8 * progress);
    SkBitmap expected_bitmap = ConstructExpectedBitmap(
        capture_rect.size(),
        /*border_color=*/BorderColor(),
        /*center_color=*/kBlack, /*border_width=*/border_width,
        /*alpha=*/progress);

    EXPECT_TRUE(cc::MatchesBitmap(
        actual_bitmap, expected_bitmap,
        cc::ManhattanDistancePixelComparator(kPxComparisonTolerance)));
  }

  border->CancelAnimation();
  EXPECT_FALSE(border->compositor_for_testing());
}

IN_PROC_BROWSER_TEST_F(BorderViewBrowserTest, FocusedWindowChange) {
  // Mimicking the user journey by clicking the button and having the WebApp set
  // the context access indicator status.
  RunTestSequence(PressButton(kGlicButtonElementId),
                  InAnyContext(WaitForShow(kGlicViewElementId)),
                  InSameContext(Steps(MoveMouseTo(kGlicViewElementId),
                                      ActivateSurface(kGlicViewElementId))));
  auto* glic_keyed_service = glic::GlicKeyedServiceFactory::GetGlicKeyedService(
      browser()->GetProfile());
  // TODO(crbug.com/390233842): We should call this in the testing web app.
  glic_keyed_service->SetContextAccessIndicator(true);

  auto* border = browser()->window()->AsBrowserView()->glic_border();
  ASSERT_TRUE(border);
  EXPECT_TRUE(border->compositor_for_testing());
  gfx::Rect capture_rect = GetContentsRectForWindow(browser());

  base::TimeTicks timestamp = base::TimeTicks::Now();

  {
    border->OnAnimationStep(timestamp);
    SkBitmap actual_bitmap = PaintBorder(border);

    SkBitmap expected_bitmap = ConstructExpectedBitmap(
        capture_rect.size(),
        /*border_color=*/BorderColor(),
        /*center_color=*/kBlack, /*border_width=*/2.f, /*alpha=*/0.f);

    EXPECT_TRUE(cc::MatchesBitmap(
        actual_bitmap, expected_bitmap,
        cc::ManhattanDistancePixelComparator(kPxComparisonTolerance)));
  }

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

  {
    // The new "t0" for the border animation in the new browser window.
    timestamp += base::Seconds(1.2345);
    new_border->OnAnimationStep(timestamp);
    SkBitmap actual_bitmap = PaintBorder(new_border);

    SkBitmap expected_bitmap = ConstructExpectedBitmap(
        capture_rect.size(),
        /*border_color=*/BorderColor(),
        /*center_color=*/kBlack, /*border_width=*/2.f, /*alpha=*/0.f);

    EXPECT_TRUE(cc::MatchesBitmap(
        actual_bitmap, expected_bitmap,
        cc::ManhattanDistancePixelComparator(kPxComparisonTolerance)));
  }
  {
    timestamp += base::Seconds(1);
    new_border->OnAnimationStep(timestamp);
    SkBitmap actual_bitmap = PaintBorder(new_border);

    float progress = sin(0.25 * M_PI);
    float border_width = 2 + (8 * progress);
    SkBitmap expected_bitmap = ConstructExpectedBitmap(
        capture_rect.size(),
        /*border_color=*/BorderColor(),
        /*center_color=*/kBlack, /*border_width=*/border_width,
        /*alpha=*/progress);
    EXPECT_TRUE(cc::MatchesBitmap(
        actual_bitmap, expected_bitmap,
        cc::ManhattanDistancePixelComparator(kPxComparisonTolerance)));
  }
}

namespace {
class BorderViewFeatureDisabledBrowserTest : public BorderViewBrowserTest {
 public:
  BorderViewFeatureDisabledBrowserTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature(features::kGlic);
  }
  ~BorderViewFeatureDisabledBrowserTest() override = default;
};
}  // namespace

// Regression test for https://crbug.com/387458471: The border is not
// initialized if the feature is disabled.
IN_PROC_BROWSER_TEST_F(BorderViewFeatureDisabledBrowserTest, NoBorder) {
  auto* border = browser()->window()->AsBrowserView()->glic_border();
  EXPECT_FALSE(border);
}

}  // namespace glic
