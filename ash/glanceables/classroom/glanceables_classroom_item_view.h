// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_ITEM_VIEW_H_
#define ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_ITEM_VIEW_H_

#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash {

// A view which shows information about a single assignment in the classroom
// glanceable.
class GlanceablesClassroomItemView : public views::FlexLayoutView {
 public:
  METADATA_HEADER(GlanceablesClassroomItemView);

  explicit GlanceablesClassroomItemView(
      const GlanceablesClassroomStudentAssignment* assignment);
  GlanceablesClassroomItemView(const GlanceablesClassroomItemView&) = delete;
  GlanceablesClassroomItemView& operator=(const GlanceablesClassroomItemView&) =
      delete;
  ~GlanceablesClassroomItemView() override;

 private:
  raw_ptr<views::Label, ExperimentalAsh> placeholder_label_ = nullptr;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_ITEM_VIEW_H_
