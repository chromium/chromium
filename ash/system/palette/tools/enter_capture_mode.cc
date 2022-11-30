// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/tools/enter_capture_mode.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/palette/palette_ids.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

EnterCaptureMode::EnterCaptureMode(Delegate* delegate)
    : CommonPaletteTool(delegate) {}

EnterCaptureMode::~EnterCaptureMode() = default;

PaletteGroup EnterCaptureMode::GetGroup() const {
  return PaletteGroup::ACTION;
}

PaletteToolId EnterCaptureMode::GetToolId() const {
  return PaletteToolId::ENTER_CAPTURE_MODE;
}

void EnterCaptureMode::OnEnable() {
  CommonPaletteTool::OnEnable();
  delegate()->DisableTool(GetToolId());
  delegate()->HidePaletteImmediately();
  CaptureModeController::Get()->Start(CaptureModeEntryType::kStylusPalette);
}

views::View* EnterCaptureMode::CreateView() {
  return CreateDefaultView(l10n_util::GetStringUTF16(
      IDS_ASH_STYLUS_TOOLS_ENTER_CAPTURE_MODE_ACTION));
}

const gfx::VectorIcon& EnterCaptureMode::GetPaletteIcon() const {
  return kCaptureModeIcon;
}

}  // namespace ash
