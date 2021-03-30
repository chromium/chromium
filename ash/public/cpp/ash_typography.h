// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASH_TYPOGRAPHY_H_
#define ASH_PUBLIC_CPP_ASH_TYPOGRAPHY_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/views/style/typography.h"

namespace ash {

enum AshTextContext {
  ASH_TEXT_CONTEXT_START = views::style::VIEWS_TEXT_CONTEXT_END,

  // A button that appears in the launcher's status area.
  CONTEXT_LAUNCHER_BUTTON = ASH_TEXT_CONTEXT_START,

  // Buttons and labels that appear in the fullscreen toast overlay UI.
  CONTEXT_TOAST_OVERLAY,

  // A button that appears within a row of the tray popup.
  CONTEXT_TRAY_POPUP_BUTTON,

  // A headline label that appears in a larger window.
  CONTEXT_HEADLINE_OVERSIZED,

  // Title label in the Sharesheet bubble. Medium weight. Usually 16pt.
  CONTEXT_SHARESHEET_BUBBLE_TITLE,

  // Body text label in the Sharesheet bubble. Meidum weight. Usually 14pt.
  CONTEXT_SHARESHEET_BUBBLE_BODY,

  // Body text label in the Sharesheet bubble. Generally appears under body
  // text. Usually 13pt.
  CONTEXT_SHARESHEET_BUBBLE_BODY_SECONDARY,

  ASH_TEXT_CONTEXT_END
};

enum AshTextStyle {
  ASH_TEXT_STYLE_START = views::style::VIEWS_TEXT_STYLE_END,

  // Used to draw attention to a section of body text such as a matched search
  // string.
  STYLE_EMPHASIZED = ASH_TEXT_STYLE_START,

  // Text styling specifically for the Chrome OS sharesheet.
  STYLE_SHARESHEET,

  ASH_TEXT_STYLE_END
};

// Sets the |details| for ash-specific text contexts. Values are only set for
// contexts specific to ash.
void ASH_PUBLIC_EXPORT
ApplyAshFontStyles(int context,
                   int style,
                   ui::ResourceBundle::FontDetails& details);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASH_TYPOGRAPHY_H_
