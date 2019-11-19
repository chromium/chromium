// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/palette_tool.h"

#include <memory>

#include "ash/assistant/util/assistant_util.h"
#include "ash/system/palette/palette_tool_manager.h"
#include "ash/system/palette/palette_utils.h"
#include "ash/system/palette/tools/capture_region_mode.h"
#include "ash/system/palette/tools/capture_screen_action.h"
#include "ash/system/palette/tools/create_note_action.h"
#include "ash/system/palette/tools/laser_pointer_mode.h"
#include "ash/system/palette/tools/magnifier_mode.h"
#include "ash/system/palette/tools/metalayer_mode.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash {

// static
void PaletteTool::RegisterToolInstances(PaletteToolManager* tool_manager) {
  tool_manager->AddTool(std::make_unique<CaptureRegionMode>(tool_manager));
  tool_manager->AddTool(std::make_unique<CaptureScreenAction>(tool_manager));
  tool_manager->AddTool(std::make_unique<CreateNoteAction>(tool_manager));
  if (assistant::util::IsGoogleDevice())
    tool_manager->AddTool(std::make_unique<MetalayerMode>(tool_manager));
  tool_manager->AddTool(std::make_unique<LaserPointerMode>(tool_manager));
  tool_manager->AddTool(std::make_unique<MagnifierMode>(tool_manager));
}

PaletteTool::PaletteTool(Delegate* delegate) : delegate_(delegate) {}

PaletteTool::~PaletteTool() = default;

void PaletteTool::OnEnable() {
  enabled_ = true;
}

void PaletteTool::OnDisable() {
  enabled_ = false;
}

const gfx::VectorIcon& PaletteTool::GetActiveTrayIcon() const {
  return gfx::kNoneIcon;
}

}  // namespace ash
