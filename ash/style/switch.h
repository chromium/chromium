// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_SWITCH_H_
#define ASH_STYLE_SWITCH_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/toggle_button.h"

namespace ash {

class ASH_EXPORT Switch : public views::ToggleButton {
  METADATA_HEADER(Switch, views::ToggleButton)

 public:
  explicit Switch(PressedCallback callback = PressedCallback());
  Switch(const Switch&) = delete;
  Switch& operator=(const Switch&) = delete;
  ~Switch() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& /*available_size*/) const override;

 private:
  // views::ToggleButton:
  SkPath GetFocusRingPath() const override;
  gfx::Rect GetTrackBounds() const override;
  gfx::Rect GetThumbBounds() const override;
};

}  // namespace ash

#endif  // ASH_STYLE_SWITCH_H_
