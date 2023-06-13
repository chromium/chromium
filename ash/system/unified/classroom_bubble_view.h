// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_VIEW_H_
#define ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/unified/glanceable_tray_child_bubble.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class ASH_EXPORT ClassroomBubbleView : public GlanceableTrayChildBubble {
 public:
  METADATA_HEADER(ClassroomBubbleView);

  // TODO(b:283370907): Add classroom glanceable contents.
  ClassroomBubbleView() = default;
  ClassroomBubbleView(const ClassroomBubbleView&) = delete;
  ClassroomBubbleView& operator-(const ClassroomBubbleView&) = delete;
  ~ClassroomBubbleView() override = default;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size CalculatePreferredSize() const override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_VIEW_H_
