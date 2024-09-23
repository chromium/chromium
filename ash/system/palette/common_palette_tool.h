// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_COMMON_PALETTE_TOOL_H_
#define ASH_SYSTEM_PALETTE_COMMON_PALETTE_TOOL_H_

#include <string>

#include "ash/system/palette/palette_tool.h"
#include "ash/system/tray/view_click_listener.h"
#include "base/memory/raw_ptr.h"

namespace gfx {
struct VectorIcon;
}

namespace ash {

class HoverHighlightView;

// A PaletteTool implementation with a standard view support.
class CommonPaletteTool : public PaletteTool, public ViewClickListener {
 protected:
  explicit CommonPaletteTool(Delegate* delegate);

  CommonPaletteTool(const CommonPaletteTool&) = delete;
  CommonPaletteTool& operator=(const CommonPaletteTool&) = delete;

  ~CommonPaletteTool() override;

  // PaletteTool:
  void OnViewDestroyed() override;
  void OnEnable() override;

  // ViewClickListener:
  void OnViewClicked(views::View* sender) override;

  // Returns the icon used in the palette tray on the left-most edge of the
  // tool. The icon will be the same as that used in the status area i.e.
  // PaletteTool::GetActiveTrayIcon().
  // TODO(michelefan): Consider using the same function to return
  // icon for palette menu and palette tray at the status area.
  virtual const gfx::VectorIcon& GetPaletteIcon() const = 0;

  // Creates a default view implementation to be returned by CreateView.
  views::View* CreateDefaultView(const std::u16string& name);

  raw_ptr<HoverHighlightView, DanglingUntriaged> highlight_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_COMMON_PALETTE_TOOL_H_
