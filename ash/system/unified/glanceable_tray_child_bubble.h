// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_CHILD_BUBBLE_H_
#define ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_CHILD_BUBBLE_H_

#include "ash/ash_export.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash {

// Child bubble of the `GlanceableTrayBubbleView`.
class ASH_EXPORT GlanceableTrayChildBubble : public TrayDetailedView {
 public:
  METADATA_HEADER(GlanceableTrayChildBubble);

  explicit GlanceableTrayChildBubble(DetailedViewDelegate* delegate);
  GlanceableTrayChildBubble(const GlanceableTrayChildBubble&) = delete;
  GlanceableTrayChildBubble& operator-(const GlanceableTrayChildBubble&) =
      delete;
  ~GlanceableTrayChildBubble() override = default;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_CHILD_BUBBLE_H_
