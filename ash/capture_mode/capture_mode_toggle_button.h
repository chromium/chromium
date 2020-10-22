// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_TOGGLE_BUTTON_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_TOGGLE_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/view_with_ink_drop.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/metadata/metadata_header_macros.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

// A toggle button that will be used in the sub views of the CaptureBarView to
// toggle between image and video capture, and between fullscreen, window, and
// region capture sources.
class ASH_EXPORT CaptureModeToggleButton
    : public ViewWithInkDrop<views::ToggleImageButton> {
 public:
  METADATA_HEADER(CaptureModeToggleButton);

  CaptureModeToggleButton(views::Button::PressedCallback callback,
                          const gfx::VectorIcon& icon);
  CaptureModeToggleButton(const CaptureModeToggleButton&) = delete;
  CaptureModeToggleButton& operator=(const CaptureModeToggleButton&) = delete;
  ~CaptureModeToggleButton() override = default;

  // views::ToggleImageButton:
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  // Called by the constructor to set the icon in both normal and toggled
  // states.
  void SetIcon(const gfx::VectorIcon& icon);

  // The color of the button background when the button is in a toggled state.
  SkColor toggled_background_color_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_TOGGLE_BUTTON_H_
