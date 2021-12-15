// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_HIGHLIGHT_BORDER_H_
#define ASH_STYLE_HIGHLIGHT_BORDER_H_

#include "ash/ash_export.h"
#include "ui/views/border.h"

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

  HighlightBorder(int corner_radius, Type type, bool use_light_colors);

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
};

}  // namespace ash

#endif  // ASH_STYLE_HIGHLIGHT_BORDER_H_
