// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_TOOLS_MAGNIFIER_MODE_H_
#define ASH_SYSTEM_PALETTE_TOOLS_MAGNIFIER_MODE_H_

#include "ash/system/palette/common_palette_tool.h"

namespace ash {

// Exposes a palette tool that lets the user enable a partial screen magnifier
// (ie, a spyglass) that dynamically appears when pressing the screen.
class MagnifierMode : public CommonPaletteTool {
 public:
  explicit MagnifierMode(Delegate* delegate);

  MagnifierMode(const MagnifierMode&) = delete;
  MagnifierMode& operator=(const MagnifierMode&) = delete;

  ~MagnifierMode() override;

 private:
  // PaletteTool overrides.
  PaletteGroup GetGroup() const override;
  PaletteToolId GetToolId() const override;
  const gfx::VectorIcon& GetActiveTrayIcon() const override;
  void OnEnable() override;
  void OnDisable() override;
  views::View* CreateView() override;

  // CommonPaletteTool overrides.
  const gfx::VectorIcon& GetPaletteIcon() const override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_TOOLS_MAGNIFIER_MODE_H_
