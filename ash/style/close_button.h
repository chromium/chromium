// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_CLOSE_BUTTON_H_
#define ASH_STYLE_CLOSE_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/style/ash_color_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view_targeter_delegate.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

// A circular ImageButton with kCloseButtonIcon inside. It has small, medium and
// large different sizes. The touch area of the small close button will be
// expanded.
class ASH_EXPORT CloseButton : public views::ImageButton,
                               public views::ViewTargeterDelegate {
  METADATA_HEADER(CloseButton, views::ImageButton)

 public:
  enum class Type {
    kSmall,
    kMedium,
    kLarge,
    kSmallFloating,
    kMediumFloating,
    kLargeFloating,
  };

  // Uses the default close icon according to the given `type` if `icon` is not
  // set by the client explicitly. Also default background and icon color ids
  // are used if they're not explicitly provided.
  // If you want to keep the button in light mode, you can provide the color ids
  // on light version.
  CloseButton(PressedCallback callback,
              Type type,
              const gfx::VectorIcon* icon = nullptr,
              ui::ColorId background_color_id = kColorAshShieldAndBase80,
              ui::ColorId icon_color_id = kColorAshButtonIconColor);
  CloseButton(const CloseButton&) = delete;
  CloseButton& operator=(const CloseButton&) = delete;
  ~CloseButton() override;

  // Returns true if the `bounding_box_` of the touch events intersect with the
  // button's bounds. Used to help enlarge the hit area of the close button.
  // Note, only necessary for `kSmall` type of CloseButton.
  bool DoesIntersectScreenRect(const gfx::Rect& screen_rect) const;

  // Resets the listener so that the listener can go out of scope.
  void ResetListener();

 private:
  // views::ImageButton:
  void OnThemeChanged() override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  const Type type_;
};

}  // namespace ash

#endif  // ASH_STYLE_CLOSE_BUTTON_H_
