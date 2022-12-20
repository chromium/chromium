// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_typography.h"

namespace ash {

void ApplyAshFontStyles(int context,
                        int style,
                        ui::ResourceBundle::FontDetails& details) {
  switch (context) {
    case CONTEXT_SEARCH_RESULT_VIEW_INLINE_ANSWER_DETAILS:
      break;
    case CONTEXT_LAUNCHER_NUDGE_LABEL:
      details.size_delta = 1;
      break;
    case CONTEXT_SHARESHEET_BUBBLE_SMALL:
      details.size_delta = -2;
      break;
    case CONTEXT_SHARESHEET_BUBBLE_BODY_SECONDARY:
      details.size_delta = 1;
      break;
    case CONTEXT_SEARCH_RESULT_CATEGORY_LABEL:
      details.size_delta = 1;
      break;
    case CONTEXT_LAUNCHER_BUTTON:
    case CONTEXT_SHARESHEET_BUBBLE_BODY:
    case CONTEXT_CALENDAR_DATE:
    case CONTEXT_SEARCH_RESULT_VIEW:
      details.size_delta = 2;
      break;
    case CONTEXT_SHARESHEET_BUBBLE_TITLE:
      details.typeface = "Google Sans";
      details.size_delta = 4;
      break;
    case CONTEXT_TRAY_POPUP_BUTTON:
      details.weight = gfx::Font::Weight::MEDIUM;
      break;
    case CONTEXT_CALENDAR_LABEL:
      details.size_delta = 6;
      break;
    case CONTEXT_HEADLINE_OVERSIZED:
      details.size_delta = 15;
      break;
    case CONTEXT_SEARCH_RESULT_BIG_TITLE:
      details.size_delta = 24;
      break;
    case CONTEXT_SEARCH_RESULT_BIG_TITLE_SUPERSCRIPT:
      details.size_delta = 6;
  }

  switch (style) {
    case STYLE_HIGHLIGHT:
      details.weight = gfx::Font::Weight::BOLD;
      break;
    case STYLE_SHARESHEET:
      DCHECK(context == CONTEXT_SHARESHEET_BUBBLE_TITLE ||
             context == CONTEXT_SHARESHEET_BUBBLE_BODY ||
             context == CONTEXT_SHARESHEET_BUBBLE_BODY_SECONDARY ||
             context == CONTEXT_SHARESHEET_BUBBLE_SMALL);
      details.weight = gfx::Font::Weight::MEDIUM;
      break;
    case STYLE_LAUNCHER:
      // TODO(https://crbug.com/1197600): Use font "Google Sans Text" when it is
      // available on Chrome OS. For now, use the default font for consistency.
      break;
  }
}

}  // namespace ash
