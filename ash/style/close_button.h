// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_CLOSE_BUTTON_H_
#define ASH_STYLE_CLOSE_BUTTON_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view_targeter_delegate.h"

namespace ash {

// A circular ImageButton with kCloseButtonIcon inside. It has small, medium and
// large different sizes. The touch area of the small close button will be
// expanded.
class CloseButton : public views::ImageButton,
                    public views::ViewTargeterDelegate {
 public:
  METADATA_HEADER(CloseButton);

  enum class Type {
    kSmall,
    kMedium,
    kLarge,
  };

  CloseButton(PressedCallback callback,
              Type type,
              bool use_light_colors = false);
  CloseButton(const CloseButton&) = delete;
  CloseButton& operator=(const CloseButton&) = delete;
  ~CloseButton() override;

  // Returns true if the `bounding_box_` of the touch events intersect with the
  // button's bounds. Used to help enlarge the hit area of the close button.
  // Note, only necessary for `kSmall` type of CloseButton.
  bool DoesIntersectScreenRect(const gfx::Rect& screen_rect) const;

 private:
  // views::ImageButton:
  void OnThemeChanged() override;
  gfx::Size CalculatePreferredSize() const override;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  const Type type_;

  // True if the button wants to use light colors when the D/L mode feature is
  // not enabled. Note, can be removed when D/L mode feature is fully launched.
  const bool use_light_colors_;
};

}  // namespace ash

#endif  // ASH_STYLE_CLOSE_BUTTON_H_
