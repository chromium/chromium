// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_TEACHER_VIEW_H_
#define ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_TEACHER_VIEW_H_

#include "ash/system/unified/classroom_bubble_base_view.h"

namespace ash {

struct GlanceablesClassroomTeacherAssignment;

class ASH_EXPORT ClassroomBubbleTeacherView : public ClassroomBubbleBaseView {
 public:
  explicit ClassroomBubbleTeacherView(DetailedViewDelegate* delegate);
  ClassroomBubbleTeacherView(const ClassroomBubbleTeacherView&) = delete;
  ClassroomBubbleTeacherView& operator=(const ClassroomBubbleTeacherView&) =
      delete;
  ~ClassroomBubbleTeacherView() override;

 private:
  // ClassroomBubbleBaseView:
  void OnSeeAllPressed() override;

  // Handle switching between assignment lists.
  void SelectedAssignmentListChanged();

  // Handles received teacher assignments by rendering them in
  // `list_container_view_`.
  void OnGetTeacherAssignments(
      std::vector<std::unique_ptr<GlanceablesClassroomTeacherAssignment>>
          assignments);

  base::WeakPtrFactory<ClassroomBubbleTeacherView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_TEACHER_VIEW_H_
