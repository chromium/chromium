// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_MENU_TOGGLE_BUTTON_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_MENU_TOGGLE_BUTTON_H_

#include "ash/style/switch.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

// A view section in the capture mode settings menu that consists of a menu item
// with a switch (toggle button).
class CaptureModeMenuToggleButton : public views::View {
  METADATA_HEADER(CaptureModeMenuToggleButton, views::View)

 public:
  CaptureModeMenuToggleButton(const gfx::VectorIcon& icon,
                              const std::u16string& label_text,
                              bool enabled,
                              views::ToggleButton::PressedCallback callback);
  CaptureModeMenuToggleButton(const CaptureModeMenuToggleButton&) = delete;
  CaptureModeMenuToggleButton& operator=(const CaptureModeMenuToggleButton&) =
      delete;
  ~CaptureModeMenuToggleButton() override;

  Switch* toggle_button() { return toggle_button_; }

  // views::View
  void OnThemeChanged() override;

 private:
  raw_ptr<views::ImageView> icon_view_;
  raw_ptr<views::Label> label_view_;

  // Toggles between enabling and disabling the capture mode demo tools feature.
  raw_ptr<Switch> toggle_button_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_MENU_TOGGLE_BUTTON_H_
