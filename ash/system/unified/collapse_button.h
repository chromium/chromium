// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_COLLAPSE_BUTTON_H_
#define ASH_SYSTEM_UNIFIED_COLLAPSE_BUTTON_H_

#include "ash/style/icon_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// The button with `kUnifiedMenuExpandIcon`. This button can be set as expanded
// or collapsed through SetExpandedAmount and the icon will be rotated on the
// `expanded_amount_`. Expanded is the default state.
class CollapseButton : public IconButton {
  METADATA_HEADER(CollapseButton, IconButton)

 public:
  explicit CollapseButton(PressedCallback callback);

  CollapseButton(const CollapseButton&) = delete;
  CollapseButton& operator=(const CollapseButton&) = delete;

  ~CollapseButton() override;

  // Change the expanded state. The icon will change.
  void SetExpandedAmount(double expanded_amount);

  // IconButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
  double expanded_amount_ = 1.0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_COLLAPSE_BUTTON_H_
