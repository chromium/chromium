// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_BUTTON_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/view_with_ink_drop.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/metadata/metadata_header_macros.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

// A view that shows a button which is part of the CaptureBarView.
class ASH_EXPORT CaptureModeButton
    : public ViewWithInkDrop<views::ImageButton> {
 public:
  METADATA_HEADER(CaptureModeButton);

  CaptureModeButton(views::Button::PressedCallback callback,
                    const gfx::VectorIcon& icon);
  CaptureModeButton(const CaptureModeButton&) = delete;
  CaptureModeButton& operator=(const CaptureModeButton&) = delete;
  ~CaptureModeButton() override = default;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_BUTTON_H_
