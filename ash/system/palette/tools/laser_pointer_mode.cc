// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/tools/laser_pointer_mode.h"

#include "ash/fast_ink/laser/laser_pointer_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/palette/palette_ids.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

LaserPointerMode::LaserPointerMode(Delegate* delegate)
    : CommonPaletteTool(delegate) {
}

LaserPointerMode::~LaserPointerMode() = default;

PaletteGroup LaserPointerMode::GetGroup() const {
  return PaletteGroup::MODE;
}

PaletteToolId LaserPointerMode::GetToolId() const {
  return PaletteToolId::LASER_POINTER;
}

void LaserPointerMode::OnEnable() {
  CommonPaletteTool::OnEnable();

  Shell::Get()->laser_pointer_controller()->SetEnabled(true);
  delegate()->HidePalette();
}

void LaserPointerMode::OnDisable() {
  CommonPaletteTool::OnDisable();

  Shell::Get()->laser_pointer_controller()->SetEnabled(false);
}

const gfx::VectorIcon& LaserPointerMode::GetActiveTrayIcon() const {
  if (AshColorProvider::Get()->IsDarkModeEnabled())
    return kPaletteTrayIconLaserPointerIcon;

  return kPaletteTrayIconLaserPointerLightModeIcon;
}

const gfx::VectorIcon& LaserPointerMode::GetPaletteIcon() const {
  if (AshColorProvider::Get()->IsDarkModeEnabled())
    return kPaletteModeLaserPointerIcon;

  return kPaletteModeLaserPointerLightModeIcon;
}

views::View* LaserPointerMode::CreateView() {
  return CreateDefaultView(
      l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_LASER_POINTER_MODE));
}
}  // namespace ash
