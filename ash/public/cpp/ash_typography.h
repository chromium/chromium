// Copyright 2017 The Chromium Authors
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

  // Text label that used in launcher nudge label. Medium weight. 13pt size.
  CONTEXT_LAUNCHER_NUDGE_LABEL,

  // A button that appears within a row of the tray popup.
  CONTEXT_TRAY_POPUP_BUTTON,

  // A headline label that appears in a larger window.
  CONTEXT_HEADLINE_OVERSIZED,

  // Title label in the Sharesheet bubble. Medium weight. Usually 16pt.
  CONTEXT_SHARESHEET_BUBBLE_TITLE,

  // Body text label in the Sharesheet bubble. Medium weight. Usually 14pt.
  CONTEXT_SHARESHEET_BUBBLE_BODY,

  // Body text label in the Sharesheet bubble. Generally appears under body
  // text. Usually 13pt.
  CONTEXT_SHARESHEET_BUBBLE_BODY_SECONDARY,

  // Text label in the Sharesheet bubble for small text. Medium weight.
  // Usually 10pt.
  CONTEXT_SHARESHEET_BUBBLE_SMALL,

  // A month label that appears in the Calendar view.
  CONTEXT_CALENDAR_LABEL,

  // Date text in the Calendar view.
  CONTEXT_CALENDAR_DATE,

  // Title and details text label used in search result view. Usually 14 pt.
  CONTEXT_SEARCH_RESULT_VIEW,

  // Big title text label used in search result view. Usually 36 pt.
  CONTEXT_SEARCH_RESULT_BIG_TITLE,

  // Big title superscript text label used in search result view. Usually 18 pt.
  CONTEXT_SEARCH_RESULT_BIG_TITLE_SUPERSCRIPT,

  // Details text label used for inline answer search result view. Usually 12pt.
  CONTEXT_SEARCH_RESULT_VIEW_INLINE_ANSWER_DETAILS,

  // Label text for search result view categories. Usually 13 pt. Appears above
  // one or more search result views.
  CONTEXT_SEARCH_RESULT_CATEGORY_LABEL,

  ASH_TEXT_CONTEXT_END
};

enum AshTextStyle {
  ASH_TEXT_STYLE_START = views::style::VIEWS_TEXT_STYLE_END,

  // Used to strongly draw attention to a section or body of text such as a
  // matched search string.
  STYLE_HIGHLIGHT = ASH_TEXT_STYLE_START,

  // Text styling specifically for the Chrome OS sharesheet.
  STYLE_SHARESHEET,

  // Text styling for launcher's search result view.
  STYLE_LAUNCHER,

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
