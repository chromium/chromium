// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_ROUNDED_LABEL_H_
#define ASH_STYLE_ROUNDED_LABEL_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"

namespace ash {

// A rounded background with a label containing text we want to display.
class RoundedLabel : public views::Label {
  METADATA_HEADER(RoundedLabel, views::Label)

 public:
  RoundedLabel(int horizontal_padding,
               int vertical_padding,
               int rounding_dp,
               int preferred_height,
               const std::u16string& text);
  RoundedLabel(const RoundedLabel&) = delete;
  RoundedLabel& operator=(const RoundedLabel&) = delete;
  ~RoundedLabel() override;

 private:
  // views::Label:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnPaintBorder(gfx::Canvas* canvas) override;

  const int rounding_dp_;
  const int preferred_height_;
};

}  // namespace ash

#endif  // ASH_STYLE_ROUNDED_LABEL_H_
