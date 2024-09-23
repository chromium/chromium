// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_TOOLS_MARKER_MODE_H_
#define ASH_SYSTEM_PALETTE_TOOLS_MARKER_MODE_H_

#include "ash/system/palette/common_palette_tool.h"

namespace gfx {
struct VectorIcon;
}

namespace views {
class View;
}

namespace ash {

// This class manages the palette tool which is a point of entry for marker
// mode. Marker mode allows users to annotate on the screen.
class MarkerMode : public CommonPaletteTool {
 public:
  explicit MarkerMode(Delegate* delegate);
  MarkerMode(const MarkerMode&) = delete;
  MarkerMode& operator=(const MarkerMode&) = delete;
  ~MarkerMode() override;

  // PaletteTool:
  PaletteGroup GetGroup() const override;
  PaletteToolId GetToolId() const override;
  void OnEnable() override;
  void OnDisable() override;
  views::View* CreateView() override;
  const gfx::VectorIcon& GetActiveTrayIcon() const override;

  // CommonPaletteTool:
  const gfx::VectorIcon& GetPaletteIcon() const override;
  void OnViewClicked(views::View* sender) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_TOOLS_MARKER_MODE_H_
