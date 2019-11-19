// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_utils.h"

#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/gfx/font_list.h"
#include "ui/views/controls/label.h"

namespace ash {

void SetupLabelForTray(views::Label* label) {
  // The text is drawn on an transparent bg, so we must disable subpixel
  // rendering.
  label->SetSubpixelRenderingEnabled(false);
  label->SetFontList(gfx::FontList().Derive(
      kTrayTextFontSizeIncrease, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
}

SkColor TrayIconColor(session_manager::SessionState session_state) {
  const bool light_icon = session_state == session_manager::SessionState::OOBE;
  return AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconPrimary,
      light_icon ? AshColorProvider::AshColorMode::kLight
                 : AshColorProvider::AshColorMode::kDark);
}

}  // namespace ash
