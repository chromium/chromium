// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_DEFAULT_FRAME_HEADER_H_
#define ASH_PUBLIC_CPP_DEFAULT_FRAME_HEADER_H_

#include <memory>

#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/frame_header.h"
#include "base/compiler_specific.h"  // override
#include "base/gtest_prod_util.h"
#include "base/macros.h"

namespace ash {

// Helper class for managing the default window header, which is used for
// Chrome apps (but not bookmark apps), for example.
class ASH_PUBLIC_EXPORT DefaultFrameHeader : public FrameHeader {
 public:
  // DefaultFrameHeader does not take ownership of any of the parameters.
  DefaultFrameHeader(views::Widget* target_widget,
                     views::View* header_view,
                     FrameCaptionButtonContainerView* caption_button_container);
  ~DefaultFrameHeader() override;

  SkColor active_frame_color_for_testing() {
    return active_frame_color_.target_color();
  }
  SkColor inactive_frame_color_for_testing() {
    return inactive_frame_color_.target_color();
  }

  void SetWidthInPixels(int width_in_pixels);

  // FrameHeader:
  void UpdateFrameColors() override;

 protected:
  // FrameHeader:
  void DoPaintHeader(gfx::Canvas* canvas) override;
  views::CaptionButtonLayoutSize GetButtonLayoutSize() const override;
  SkColor GetTitleColor() const override;
  SkColor GetCurrentFrameColor() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(DefaultFrameHeaderTest, FrameColors);

  // Returns the window of the target widget.
  aura::Window* GetTargetWindow();

  gfx::SlideAnimation* GetAnimationForActiveFrameColorForTest();
  SkColor GetActiveFrameColorForPaintForTest();

  // A utility class to animate color value.
  class ColorAnimator {
   public:
    explicit ColorAnimator(gfx::AnimationDelegate* delegate);
    ~ColorAnimator();

    void SetTargetColor(SkColor target);
    SkColor target_color() const { return target_color_; }
    SkColor GetCurrentColor();
    float get_value() const { return animation_.GetCurrentValue(); }

    gfx::SlideAnimation* animation() { return &animation_; }

   private:
    gfx::SlideAnimation animation_;
    SkColor start_color_ = kDefaultFrameColor;
    SkColor target_color_ = kDefaultFrameColor;
    SkColor current_color_ = kDefaultFrameColor;

    DISALLOW_COPY_AND_ASSIGN(ColorAnimator);
  };

  ColorAnimator active_frame_color_;
  ColorAnimator inactive_frame_color_;

  int width_in_pixels_ = -1;

  DISALLOW_COPY_AND_ASSIGN(DefaultFrameHeader);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_DEFAULT_FRAME_HEADER_H_
