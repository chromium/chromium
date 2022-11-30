// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_TOOLS_ENTER_CAPTURE_MODE_H_
#define ASH_SYSTEM_PALETTE_TOOLS_ENTER_CAPTURE_MODE_H_

#include "ash/system/palette/common_palette_tool.h"

namespace ash {

// This class manages the palette tool which is a point of entry for capture
// mode. Capture mode allows users to take screenshots and record videos.
class EnterCaptureMode : public CommonPaletteTool {
 public:
  explicit EnterCaptureMode(Delegate* delegate);
  EnterCaptureMode(const EnterCaptureMode&) = delete;
  EnterCaptureMode& operator=(const EnterCaptureMode&) = delete;
  ~EnterCaptureMode() override;

  // CommonPaletteTool:
  PaletteGroup GetGroup() const override;
  PaletteToolId GetToolId() const override;
  void OnEnable() override;
  views::View* CreateView() override;
  const gfx::VectorIcon& GetPaletteIcon() const override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_TOOLS_ENTER_CAPTURE_MODE_H_
