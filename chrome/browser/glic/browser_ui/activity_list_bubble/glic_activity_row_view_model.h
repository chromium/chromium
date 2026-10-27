// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_ACTIVITY_LIST_BUBBLE_GLIC_ACTIVITY_ROW_VIEW_MODEL_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_ACTIVITY_LIST_BUBBLE_GLIC_ACTIVITY_ROW_VIEW_MODEL_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/actor/core/task_id.h"
#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_types.h"

namespace glic {

// View model representing a single row entry in the GlicActivityListBubble.
// Decouples UI rendering (GlicActivityListBubbleRowButton) from domain-specific
// activity state.
struct GlicActivityRowViewModel {
  GlicActivityRowViewModel();
  GlicActivityRowViewModel(const GlicActivityRowViewModel&);
  GlicActivityRowViewModel(GlicActivityRowViewModel&&);
  GlicActivityRowViewModel& operator=(const GlicActivityRowViewModel&);
  GlicActivityRowViewModel& operator=(GlicActivityRowViewModel&&);
  ~GlicActivityRowViewModel();

  // Optional domain identifiers for the underlying activity.
  std::optional<actor::TaskId> task_id;
  std::optional<std::string> conversation_id;

  // Callback invoked when the user clicks the row or its redirect icon.
  base::RepeatingClosure on_clicked;

  // Primary icon displayed on the left of the row.
  raw_ptr<const gfx::VectorIcon> icon = nullptr;

  // Color for the primary icon and subtitle text.
  ui::ColorId icon_and_subtitle_color = ui::ColorId();

  // Main title text for the row.
  std::u16string title;

  // Subtitle / status text displayed below the title.
  std::u16string subtitle;

  // Whether the row button is enabled and clickable.
  bool is_enabled = true;

  // Whether the row has an associated active browser tab. When true, hovering
  // the row reveals the open-tab redirect icon.
  bool has_tab = false;

  // Android tab ID associated with this row, or -1 if none.
  int tab_id = -1;

  // Whether this activity requires user review/attention.
  bool needs_review = false;

  // Whether this activity has already been viewed/seen by the user.
  bool has_been_seen = false;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_ACTIVITY_LIST_BUBBLE_GLIC_ACTIVITY_ROW_VIEW_MODEL_H_
