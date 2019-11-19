// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/common_palette_tool.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/system/palette/palette_ids.h"
#include "ash/system/palette/palette_tool_manager.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/view_click_listener.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace {

void AddHistogramTimes(PaletteToolId id, base::TimeDelta duration) {
  if (id == PaletteToolId::LASER_POINTER) {
    UMA_HISTOGRAM_CUSTOM_TIMES("Ash.Shelf.Palette.InLaserPointerMode", duration,
                               base::TimeDelta::FromMilliseconds(100),
                               base::TimeDelta::FromHours(1), 50);
  } else if (id == PaletteToolId::MAGNIFY) {
    UMA_HISTOGRAM_CUSTOM_TIMES("Ash.Shelf.Palette.InMagnifyMode", duration,
                               base::TimeDelta::FromMilliseconds(100),
                               base::TimeDelta::FromHours(1), 50);
  } else if (id == PaletteToolId::METALAYER) {
    UMA_HISTOGRAM_CUSTOM_TIMES("Ash.Shelf.Palette.InAssistantMode", duration,
                               base::TimeDelta::FromMilliseconds(100),
                               base::TimeDelta::FromHours(1), 50);
  }
}

}  // namespace

CommonPaletteTool::CommonPaletteTool(Delegate* delegate)
    : PaletteTool(delegate) {}

CommonPaletteTool::~CommonPaletteTool() = default;

void CommonPaletteTool::OnViewDestroyed() {
  highlight_view_ = nullptr;
}

void CommonPaletteTool::OnEnable() {
  PaletteTool::OnEnable();
  start_time_ = base::TimeTicks::Now();
}

void CommonPaletteTool::OnDisable() {
  PaletteTool::OnDisable();
  AddHistogramTimes(GetToolId(), base::TimeTicks::Now() - start_time_);
}

void CommonPaletteTool::OnViewClicked(views::View* sender) {
  // The tool should always be disabled when we click it because the bubble
  // which houses this view is automatically closed when the tool is first
  // enabled. Then, to open the bubble again we have to click on the palette
  // tray twice, and the first click will disable any active tools.
  DCHECK(!enabled());

  delegate()->RecordPaletteOptionsUsage(
      PaletteToolIdToPaletteTrayOptions(GetToolId()),
      PaletteInvocationMethod::MENU);
  delegate()->EnableTool(GetToolId());
}

views::View* CommonPaletteTool::CreateDefaultView(const base::string16& name) {
  gfx::ImageSkia icon =
      CreateVectorIcon(GetPaletteIcon(), kMenuIconSize, gfx::kChromeIconGrey);
  highlight_view_ = new HoverHighlightView(this, false /* use_unified_theme */);
  highlight_view_->AddIconAndLabel(icon, name);
  return highlight_view_;
}

}  // namespace ash
