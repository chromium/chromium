// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_HIGHLIGHT_BORDER_H_
#define ASH_STYLE_HIGHLIGHT_BORDER_H_

#include "ash/ash_export.h"
#include "ui/views/border.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

// A rounded rectangle border that has inner (highlight) and outer color.
// Useful when go/cros-launcher-spec mentions "BorderHighlight".
class ASH_EXPORT HighlightBorder : public views::Border {
 public:
  enum class Type {
    // A higher contrast highlight border than the `kHighlightBorder2` used
    // for floating components that do not have a shield below.
    kHighlightBorder1,
    // A less contrast highlight border for components that float above a
    // shield.
    kHighlightBorder2,
  };

  // The type of insets created by this highlight border. The insets shrink the
  // content view.
  enum class InsetsType {
    // The border does not add insets that shrink the content, so the content
    // around the edge of the view can be overlapped with the border.
    kNoInsets,
    // Insetting by half of the border size pushes the content inside the outer
    // color of the border so the content bounds is surrounded by outer color
    // of the border.
    kHalfInsets,
    // Insetting by full border size makes sure that contents in the view
    // start inside the inner color of the highlight border, so there is no
    // overlap between content.
    kFullInsets,
  };

  // Paints the highlight border onto `canvas`. Note that directly using this
  // function won't set the insets on any view so it acts like setting kNoInsets
  // when using HighlightBorder class.
  static void PaintBorderToCanvas(gfx::Canvas* canvas,
                                  const gfx::Rect& bounds,
                                  int corner_radius,
                                  Type type,
                                  bool use_light_colors);

  HighlightBorder(int corner_radius,
                  Type type,
                  bool use_light_colors,
                  InsetsType insets_type = InsetsType::kNoInsets);

  HighlightBorder(const HighlightBorder&) = delete;
  HighlightBorder& operator=(const HighlightBorder&) = delete;

  ~HighlightBorder() override = default;

  // views::Border:
  void Paint(const views::View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  const int corner_radius_;
  const Type type_;

  // True if the border should use light colors when the D/L mode feature is
  // not enabled.
  const bool use_light_colors_;

  // Used by `GetInsets()` to calculate the insets.
  const InsetsType insets_type_;
};

}  // namespace ash

#endif  // ASH_STYLE_HIGHLIGHT_BORDER_H_
