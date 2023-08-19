// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_ITEM_VIEW_H_
#define ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "base/functional/callback_forward.h"
#include "ui/views/controls/button/button.h"

namespace ash {

struct GlanceablesClassroomAssignment;

// A view which shows information about a single assignment in the classroom
// glanceable.
class ASH_EXPORT GlanceablesClassroomItemView : public views::Button {
 public:
  METADATA_HEADER(GlanceablesClassroomItemView);

  GlanceablesClassroomItemView(const GlanceablesClassroomAssignment* assignment,
                               base::RepeatingClosure pressed_callback,
                               size_t item_index,
                               size_t last_item_index);

  GlanceablesClassroomItemView(const GlanceablesClassroomItemView&) = delete;
  GlanceablesClassroomItemView& operator=(const GlanceablesClassroomItemView&) =
      delete;
  ~GlanceablesClassroomItemView() override;

  // views::Button:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void Layout() override;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_ITEM_VIEW_H_
