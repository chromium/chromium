// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/common_palette_tool.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/palette/palette_ids.h"
#include "ash/system/palette/palette_tool_manager.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/view_click_listener.h"
#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/label.h"

namespace ash {

CommonPaletteTool::CommonPaletteTool(Delegate* delegate)
    : PaletteTool(delegate) {}

CommonPaletteTool::~CommonPaletteTool() = default;

void CommonPaletteTool::OnViewDestroyed() {
  highlight_view_ = nullptr;
}

void CommonPaletteTool::OnEnable() {
  PaletteTool::OnEnable();
}

void CommonPaletteTool::OnViewClicked(views::View* sender) {
  // The tool should always be disabled when we click it because the bubble
  // which houses this view is automatically closed when the tool is first
  // enabled. Then, to open the bubble again we have to click on the palette
  // tray twice, and the first click will disable any active tools.
  DCHECK(!enabled());
  delegate()->EnableTool(GetToolId());
}

views::View* CommonPaletteTool::CreateDefaultView(const std::u16string& name) {
  SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
  gfx::ImageSkia icon =
      CreateVectorIcon(GetPaletteIcon(), kMenuIconSize, icon_color);
  highlight_view_ = new HoverHighlightView(this);
  highlight_view_->AddIconAndLabel(icon, name);
  return highlight_view_;
}

}  // namespace ash
