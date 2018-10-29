// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_H_
#define ASH_PUBLIC_CPP_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/caption_buttons/caption_button_types.h"
#include "base/macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"

namespace gfx {
class SlideAnimation;
struct VectorIcon;
}  // namespace gfx

namespace ash {

// Base class for the window caption buttons (minimize, maximize, restore,
// close).
class ASH_PUBLIC_EXPORT FrameCaptionButton : public views::Button {
 public:
  enum Animate { ANIMATE_YES, ANIMATE_NO };

  enum class ColorMode {
    kDefault,  // Most windows.
    kThemed,   // Windows that have been themed by PWA manifest.
  };

  static const char kViewClassName[];

  FrameCaptionButton(views::ButtonListener* listener,
                     CaptionButtonIcon icon,
                     int hit_test_type);
  ~FrameCaptionButton() override;

  // Gets the color to use for a frame caption button while a theme color is
  // set.
  static SkColor GetButtonColor(ColorMode color_mode, SkColor background_color);

  // Gets the alpha ratio for the colors of inactive frame caption buttons.
  static float GetInactiveButtonColorAlphaRatio();

  // Sets the image to use to paint the button. If |animate| is ANIMATE_YES,
  // the button crossfades to the new visuals. If the image matches the one
  // currently used by the button and |animate| is ANIMATE_NO, the crossfade
  // animation is progressed to the end.
  void SetImage(CaptionButtonIcon icon,
                Animate animate,
                const gfx::VectorIcon& icon_image);

  // Returns true if the button is crossfading to new visuals set in
  // SetImage().
  bool IsAnimatingImageSwap() const;

  // Sets the alpha to use for painting. Used to animate visibility changes.
  void SetAlpha(int alpha);

  // views::Button:
  const char* GetClassName() const override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  views::PaintInfo::ScaleType GetPaintScaleType() const override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;

  void SetBackgroundColor(SkColor background_color);
  void SetColorMode(ColorMode color_mode);

  void set_paint_as_active(bool paint_as_active) {
    paint_as_active_ = paint_as_active;
  }

  bool paint_as_active() { return paint_as_active_; }

  CaptionButtonIcon icon() const { return icon_; }

  const gfx::ImageSkia& icon_image() { return icon_image_; }

  const gfx::VectorIcon* icon_definition_for_test() const {
    return icon_definition_;
  }

 protected:
  // views::Button override:
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
  // Determines what alpha to use for the icon based on animation and
  // active state.
  int GetAlphaForIcon(int base_alpha) const;

  void UpdateInkDropBaseColor();

  // The button's current icon.
  CaptionButtonIcon icon_;

  // The current background color.
  SkColor background_color_;

  // The algorithm to determine button colors.
  ColorMode color_mode_;

  // Whether the button should be painted as active.
  bool paint_as_active_;

  // Current alpha to use for painting.
  int alpha_;

  // The image id (kept for the purposes of testing) and image used to paint the
  // button's icon.
  const gfx::VectorIcon* icon_definition_ = nullptr;
  gfx::ImageSkia icon_image_;

  // The icon image to crossfade from.
  gfx::ImageSkia crossfade_icon_image_;

  // Crossfade animation started when the button's images are changed by
  // SetImage().
  std::unique_ptr<gfx::SlideAnimation> swap_images_animation_;

  DISALLOW_COPY_AND_ASSIGN(FrameCaptionButton);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_H_
