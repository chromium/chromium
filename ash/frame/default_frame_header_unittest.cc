// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/default_frame_header.h"

#include <memory>

#include "ash/public/cpp/caption_buttons/frame_back_button.h"
#include "ash/public/cpp/caption_buttons/frame_caption_button_container_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_util.h"
#include "base/i18n/rtl.h"
#include "base/test/icu_test_util.h"
#include "ui/aura/window.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/test/test_views.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

using views::NonClientFrameView;
using views::Widget;

namespace ash {

using DefaultFrameHeaderTest = AshTestBase;

// Ensure the title text is vertically aligned with the window icon.
TEST_F(DefaultFrameHeaderTest, TitleIconAlignment) {
  std::unique_ptr<Widget> widget = CreateTestWidget(
      nullptr, desks_util::GetActiveDeskContainerId(), gfx::Rect(1, 2, 3, 4));
  FrameCaptionButtonContainerView container(widget.get());
  views::StaticSizedView window_icon(gfx::Size(16, 16));
  window_icon.SetBounds(0, 0, 16, 16);
  widget->SetBounds(gfx::Rect(0, 0, 500, 500));
  widget->Show();

  DefaultFrameHeader frame_header(
      widget.get(), widget->non_client_view()->frame_view(), &container);
  frame_header.SetLeftHeaderView(&window_icon);
  frame_header.LayoutHeader();
  gfx::Rect title_bounds = frame_header.GetTitleBounds();
  EXPECT_EQ(window_icon.bounds().CenterPoint().y(),
            title_bounds.CenterPoint().y());
}

TEST_F(DefaultFrameHeaderTest, BackButtonAlignment) {
  std::unique_ptr<Widget> widget = CreateTestWidget(
      nullptr, desks_util::GetActiveDeskContainerId(), gfx::Rect(1, 2, 3, 4));
  FrameCaptionButtonContainerView container(widget.get());
  FrameBackButton back;

  DefaultFrameHeader frame_header(
      widget.get(), widget->non_client_view()->frame_view(), &container);
  frame_header.SetBackButton(&back);
  frame_header.LayoutHeader();
  gfx::Rect title_bounds = frame_header.GetTitleBounds();
  // The back button should be positioned at the left edge, and
  // vertically centered.
  EXPECT_EQ(back.bounds().CenterPoint().y(), title_bounds.CenterPoint().y());
  EXPECT_EQ(0, back.bounds().x());
}

TEST_F(DefaultFrameHeaderTest, MinimumHeaderWidthRTL) {
  base::test::ScopedRestoreICUDefaultLocale restore_locale;
  std::unique_ptr<Widget> widget = CreateTestWidget(
      nullptr, desks_util::GetActiveDeskContainerId(), gfx::Rect(1, 2, 3, 4));
  FrameCaptionButtonContainerView container(widget.get());

  DefaultFrameHeader frame_header(
      widget.get(), widget->non_client_view()->frame_view(), &container);
  frame_header.LayoutHeader();
  int ltr_minimum_width = frame_header.GetMinimumHeaderWidth();
  base::i18n::SetRTLForTesting(true);
  frame_header.LayoutHeader();
  int rtl_minimum_width = frame_header.GetMinimumHeaderWidth();
  EXPECT_EQ(ltr_minimum_width, rtl_minimum_width);
}

// Ensure the right frame colors are used.
TEST_F(DefaultFrameHeaderTest, FrameColors) {
  std::unique_ptr<Widget> widget = CreateTestWidget(
      nullptr, desks_util::GetActiveDeskContainerId(), gfx::Rect(1, 2, 3, 4));
  FrameCaptionButtonContainerView container(widget.get());
  views::StaticSizedView window_icon(gfx::Size(16, 16));
  window_icon.SetBounds(0, 0, 16, 16);
  widget->SetBounds(gfx::Rect(0, 0, 500, 500));
  widget->Show();

  DefaultFrameHeader frame_header(
      widget.get(), widget->non_client_view()->frame_view(), &container);
  // Check frame color is sensitive to mode.
  SkColor active = SkColorSetRGB(70, 70, 70);
  SkColor inactive = SkColorSetRGB(200, 200, 200);
  widget->GetNativeWindow()->SetProperty(kFrameActiveColorKey, active);
  widget->GetNativeWindow()->SetProperty(kFrameInactiveColorKey, inactive);
  frame_header.UpdateFrameColors();
  frame_header.mode_ = FrameHeader::MODE_ACTIVE;
  EXPECT_EQ(active, frame_header.GetCurrentFrameColor());
  frame_header.mode_ = FrameHeader::MODE_INACTIVE;
  EXPECT_EQ(inactive, frame_header.GetCurrentFrameColor());
  EXPECT_EQ(active, frame_header.GetActiveFrameColorForPaintForTest());

  // Update to the new value which has no blue, which should animate.
  frame_header.mode_ = FrameHeader::MODE_ACTIVE;
  SkColor new_active = SkColorSetRGB(70, 70, 0);
  widget->GetNativeWindow()->SetProperty(kFrameActiveColorKey, new_active);
  frame_header.UpdateFrameColors();

  gfx::SlideAnimation* animation =
      frame_header.GetAnimationForActiveFrameColorForTest();
  gfx::AnimationTestApi test_api(animation);

  // animate half way through.
  base::TimeTicks now = base::TimeTicks::Now();
  test_api.SetStartTime(now);
  test_api.Step(now + base::TimeDelta::FromMilliseconds(120));

  // GetCurrentFrameColor should return the target color.
  EXPECT_EQ(new_active, frame_header.GetCurrentFrameColor());

  // The color used for paint should be somewhere between 0 and 70.
  SkColor new_active_for_paint =
      frame_header.GetActiveFrameColorForPaintForTest();
  EXPECT_NE(new_active, new_active_for_paint);
  EXPECT_EQ(53u, SkColorGetB(new_active_for_paint));

  // Now update to the new value which is full blue.
  SkColor new_new_active = SkColorSetRGB(70, 70, 255);
  widget->GetNativeWindow()->SetProperty(kFrameActiveColorKey, new_new_active);
  frame_header.UpdateFrameColors();

  now = base::TimeTicks::Now();
  test_api.SetStartTime(now);
  test_api.Step(now + base::TimeDelta::FromMilliseconds(20));

  // Again, GetCurrentFrameColor should return the target color.
  EXPECT_EQ(new_new_active, frame_header.GetCurrentFrameColor());

  // The start value should be the previous paint color, so it should be
  // near 53.
  SkColor new_new_active_for_paint =
      frame_header.GetActiveFrameColorForPaintForTest();
  EXPECT_NE(new_active_for_paint, new_new_active_for_paint);
  EXPECT_EQ(54u, SkColorGetB(new_new_active_for_paint));
}

}  // namespace ash
