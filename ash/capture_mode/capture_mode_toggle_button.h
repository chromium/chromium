// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_TOGGLE_BUTTON_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_TOGGLE_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

// A toggle button that will be used in the sub views of the CaptureBarView to
// toggle between image and video capture, and between fullscreen, window, and
// region capture sources.
class ASH_EXPORT CaptureModeToggleButton
    : public views::ToggleImageButton,
      public CaptureModeSessionFocusCycler::HighlightableView {
 public:
  METADATA_HEADER(CaptureModeToggleButton);

  CaptureModeToggleButton(views::Button::PressedCallback callback,
                          const gfx::VectorIcon& icon);
  CaptureModeToggleButton(const CaptureModeToggleButton&) = delete;
  CaptureModeToggleButton& operator=(const CaptureModeToggleButton&) = delete;
  ~CaptureModeToggleButton() override = default;

  void set_toggled_background_color(SkColor color) {
    toggled_background_color_ = color;
  }

  // views::ToggleImageButton:
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // CaptureModeSessionFocusCycler::HighlightableView:
  views::View* GetView() override;

 private:
  // Called by the constructor to set the icon in both normal and toggled
  // states.
  void SetIcon(const gfx::VectorIcon& icon);

  // The color of the button background when the button is in a toggled state.
  SkColor toggled_background_color_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_TOGGLE_BUTTON_H_
