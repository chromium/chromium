// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_STYLE_UTIL_H_
#define ASH_STYLE_STYLE_UTIL_H_

#include "ash/ash_export.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/animation/ink_drop_highlight.h"

namespace views {
class Button;
class FocusRing;
class View;
}  // namespace views

namespace ash {

class ASH_EXPORT StyleUtil {
 public:
  // InkDrop attributes that can be configured through
  // ConfigureInkDropAttributes. Including the base color, inkdrop and highlight
  // opacity.
  enum InkDropAttributes {
    kBaseColor = 1,
    kInkDropOpacity = 1 << 1,
    kHighlightOpacity = 1 << 2
  };

  // Sets attributes(e.g, insets) for creating the inkdrop ripple. Note, A
  // FloodFillInkDropRipple will be created for the given `host`.
  static void SetRippleParams(
      views::View* host,
      const gfx::Insets& insets,
      SkColor background_color = gfx::kPlaceholderColor);

  // TODO: Remove TrayPopupUtils::ConfigureTrayPopupButton and migrate all its
  // clients to this function.
  // Sets up the inkdrop for the given `button`. Including setting the callback
  // for InkDrop, Ripple, Highlight. Inside the callback functions, they will
  // setup whether to show the highlight on hover or focus, inkdrop color,
  // opacity etc.
  static void SetUpInkDropForButton(
      views::Button* button,
      const gfx::Insets& ripple_insets,
      bool highlight_on_hover,
      bool highlight_on_focus,
      SkColor background_color = gfx::kPlaceholderColor);

  // Configures the InkDropAttributes for the given `view` based on
  // `attributes`. Note, `attributes` is a bitmask from InkDropAttributes.
  static void ConfigureInkDropAttributes(
      views::View* view,
      uint32_t attributes,
      SkColor background_color = gfx::kPlaceholderColor);

  // Sets up the focus ring and its color for `view`. `halo_inset` is the
  // adjustment from the visible border of the host view to render the focus
  // ring. If it is not given, then the default (-0.5 * thickness) will be used.
  static views::FocusRing* SetUpFocusRingForView(
      views::View* view,
      absl::optional<int> halo_inset = absl::nullopt);

 private:
  StyleUtil() = default;
  StyleUtil(const StyleUtil&) = delete;
  StyleUtil& operator=(const StyleUtil&) = delete;
  ~StyleUtil() = default;
};

}  // namespace ash

#endif  // ASH_STYLE_STYLE_UTIL_H_
