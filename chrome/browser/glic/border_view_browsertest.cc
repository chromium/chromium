// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _USE_MATH_DEFINES  // To get M_PI on Windows.

#include "chrome/browser/glic/border_view.h"

#include <math.h>

#include "cc/test/pixel_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"

namespace glic {

namespace {
class BorderViewBrowserTest : public InProcessBrowserTest {
 public:
  BorderViewBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton},
        /*disabled_features=*/{});
  }
  ~BorderViewBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kGlicGuestURL,
                                    url::kAboutBlankURL);
  }

  static SkBitmap ConstructExpectedBitmap(const gfx::Size& size,
                                          SkColor border_color,
                                          SkColor center_color,
                                          int border_width,
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
  SkColor background_color = SkColors::kBlack.toSkColor();
  SkColor border_color =
      browser()->GetBrowserView().GetColorProvider()->GetColor(
          ui::kColorSysPrimary);

  border->StartAnimation();

  // Manually stepping the animation code to mimic the behavior of the
  // compositor. As a part of crbug.com/384712084, testing via requesting
  // screenshot from the browser window was explored however, was failed due to
  // test falkiness (crbug.com/387386303).

  base::TimeTicks timestamp = base::TimeTicks::Now();

  // Note: the following is based on having the animation duration = 2 seconds.
  // timestamp = 0 (now).
  {
    border->OnAnimationStep(timestamp);
    gfx::Canvas canvas(capture_rect.size(), /*image_scale=*/1.0f,
                       /*is_opaque=*/true);
    canvas.DrawColor(background_color);
    border->OnPaint(&canvas);
    SkBitmap actual_bitmap = canvas.GetBitmap();

    SkBitmap expected_bitmap = ConstructExpectedBitmap(
        capture_rect.size(),
        /*border_color=*/border_color,
        /*center_color=*/background_color, /*border_width=*/2, /*alpha=*/0);

    EXPECT_TRUE(cc::MatchesBitmap(actual_bitmap, expected_bitmap,
                                  cc::ExactPixelComparator()));
  }

  // timestamp = 0.5 seconds.
  {
    timestamp += base::Seconds(0.5);
    border->OnAnimationStep(timestamp);
    gfx::Canvas canvas(capture_rect.size(), /*image_scale=*/1.0f,
                       /*is_opaque=*/true);
    canvas.DrawColor(background_color);
    border->OnPaint(&canvas);
    SkBitmap actual_bitmap = canvas.GetBitmap();

    // The sin input is calculated as:
    // (since_first_frame * M_PI) / (`kAnimationDuration` * 2)
    // At timestamp = 0.5 and with kAnimationDuration = 2,
    // we have: (0.5 * M_PI) / (2 * 2) = 0.125 * M_PI.
    float progress = sin(0.125 * M_PI);

    // The border width is calculated as:
    // `kBorderWidthMin` + ((`kBorderWidthMax` - `kBorderWidthMin`) *
    // `progress`).
    float border_width = 2 + (8 * progress);
    SkBitmap expected_bitmap = ConstructExpectedBitmap(
        capture_rect.size(),
        /*border_color=*/border_color,
        /*center_color=*/background_color, /*border_width=*/border_width,
        /*alpha=*/progress);

    EXPECT_TRUE(cc::MatchesBitmap(actual_bitmap, expected_bitmap,
                                  cc::ExactPixelComparator()));
  }

  // timestamp = 2 seconds.
  {
    timestamp += base::Seconds(1.5);
    border->OnAnimationStep(timestamp);
    gfx::Canvas canvas(capture_rect.size(), /*image_scale=*/1.0f,
                       /*is_opaque=*/true);
    canvas.DrawColor(background_color);
    border->OnPaint(&canvas);
    SkBitmap actual_bitmap = canvas.GetBitmap();

    float progress = sin(0.5 * M_PI);
    float border_width = 2 + (8 * progress);
    SkBitmap expected_bitmap = ConstructExpectedBitmap(
        capture_rect.size(),
        /*border_color=*/border_color,
        /*center_color=*/background_color, /*border_width=*/border_width,
        /*alpha=*/progress);

    EXPECT_TRUE(cc::MatchesBitmap(actual_bitmap, expected_bitmap,
                                  cc::ExactPixelComparator()));
  }

  // timestamp = 4 seconds; stable state.
  {
    timestamp += base::Seconds(2);
    border->OnAnimationStep(timestamp);
    gfx::Canvas canvas(capture_rect.size(), /*image_scale=*/1.0f,
                       /*is_opaque=*/true);
    canvas.DrawColor(background_color);
    border->OnPaint(&canvas);
    SkBitmap actual_bitmap = canvas.GetBitmap();

    SkBitmap expected_bitmap = ConstructExpectedBitmap(
        capture_rect.size(),
        /*border_color=*/border_color,
        /*center_color=*/background_color, /*border_width=*/10, /*alpha=*/1);

    EXPECT_TRUE(cc::MatchesBitmap(actual_bitmap, expected_bitmap,
                                  cc::ExactPixelComparator()));
  }

  border->CancelAnimation();
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
