// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_typography.h"

namespace ash {

void ApplyAshFontStyles(int context,
                        int style,
                        int* size_delta,
                        gfx::Font::Weight* font_weight,
                        std::string* typeface) {
  switch (context) {
    case CONTEXT_SHARESHEET_BUBBLE_BODY_SECONDARY:
      *size_delta = 1;
      break;
    case CONTEXT_LAUNCHER_BUTTON:
    case CONTEXT_SHARESHEET_BUBBLE_BODY:
      *size_delta = 2;
      break;
    case CONTEXT_TOAST_OVERLAY:
      *size_delta = 3;
      break;
    case CONTEXT_SHARESHEET_BUBBLE_TITLE:
      *size_delta = 4;
      *typeface = "Google Sans";
      break;
    case CONTEXT_TRAY_POPUP_BUTTON:
      *font_weight = gfx::Font::Weight::MEDIUM;
      break;
    case CONTEXT_HEADLINE_OVERSIZED:
      *size_delta = 15;
      break;
  }

  switch (style) {
    case STYLE_EMPHASIZED:
      *font_weight = gfx::Font::Weight::SEMIBOLD;
      break;
    case STYLE_SHARESHEET:
      DCHECK(context == CONTEXT_SHARESHEET_BUBBLE_TITLE ||
             context == CONTEXT_SHARESHEET_BUBBLE_BODY ||
             context == CONTEXT_SHARESHEET_BUBBLE_BODY_SECONDARY);
      *font_weight = gfx::Font::Weight::MEDIUM;
      break;
  }
}

}  // namespace ash
