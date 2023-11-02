// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_BUTTON_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class FocusRing;
}  // namespace views

namespace ash {

// A view that shows a button which is part of the CaptureBarView.
class ASH_EXPORT CaptureModeButton
    : public views::ImageButton,
      public CaptureModeSessionFocusCycler::HighlightableView {
 public:
  METADATA_HEADER(CaptureModeButton);

  CaptureModeButton(views::Button::PressedCallback callback,
                    const gfx::VectorIcon& icon);
  CaptureModeButton(const CaptureModeButton&) = delete;
  CaptureModeButton& operator=(const CaptureModeButton&) = delete;
  ~CaptureModeButton() override = default;

  // Common configuration for CaptureModeButton and CaptureModeToggleButton,
  // such as InkDrop, preferred size, border, etc.
  static void ConfigureButton(views::ImageButton* button,
                              views::FocusRing* focus_ring);

  // CaptureModeSessionFocusCycler::HighlightableView:
  views::View* GetView() override;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_BUTTON_H_
