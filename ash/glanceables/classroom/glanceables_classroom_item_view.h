// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_ITEM_VIEW_H_
#define ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash {

struct GlanceablesClassroomStudentAssignment;

// A view which shows information about a single assignment in the classroom
// glanceable.
class ASH_EXPORT GlanceablesClassroomItemView : public views::FlexLayoutView {
 public:
  METADATA_HEADER(GlanceablesClassroomItemView);

  // Known view ids.
  static constexpr int kIconViewId = 1;
  static constexpr int kCourseWorkTitleLabelId = 2;
  static constexpr int kCourseTitleLabelId = 3;
  static constexpr int kDueDateLabelId = 4;
  static constexpr int kDueTimeLabelId = 5;

  explicit GlanceablesClassroomItemView(
      const GlanceablesClassroomStudentAssignment* assignment);
  GlanceablesClassroomItemView(const GlanceablesClassroomItemView&) = delete;
  GlanceablesClassroomItemView& operator=(const GlanceablesClassroomItemView&) =
      delete;
  ~GlanceablesClassroomItemView() override;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_ITEM_VIEW_H_
