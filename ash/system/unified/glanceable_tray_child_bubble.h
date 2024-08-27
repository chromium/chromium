// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_CHILD_BUBBLE_H_
#define ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_CHILD_BUBBLE_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

// Child bubble of the `GlanceableTrayBubbleView`.
class ASH_EXPORT GlanceableTrayChildBubble : public views::View {
  METADATA_HEADER(GlanceableTrayChildBubble, views::View)

 public:
  // `use_glanceables_container_style` - whether the bubble should be styled as
  // a bubble in the glanceables container. `CalendarView` is a
  // `GlanceablesTrayChildBubble` and if the glanceblae view flag is
  // not enabled, the calendar view will be added to the
  // `UnifiedSystemTrayBubble` which has its own styling.
  explicit GlanceableTrayChildBubble(bool use_glanceables_container_style);
  GlanceableTrayChildBubble(const GlanceableTrayChildBubble&) = delete;
  GlanceableTrayChildBubble& operator-(const GlanceableTrayChildBubble&) =
      delete;
  ~GlanceableTrayChildBubble() override = default;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_CHILD_BUBBLE_H_
