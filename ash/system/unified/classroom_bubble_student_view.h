// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_STUDENT_VIEW_H_
#define ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_STUDENT_VIEW_H_

#include "ash/system/unified/classroom_bubble_base_view.h"

namespace ash {

struct GlanceablesClassroomStudentAssignment;

// class ClassroomBubbleStudentView : public views::View {
class ASH_EXPORT ClassroomBubbleStudentView : public ClassroomBubbleBaseView {
 public:
  explicit ClassroomBubbleStudentView(DetailedViewDelegate* delegate);
  ClassroomBubbleStudentView(const ClassroomBubbleStudentView&) = delete;
  ClassroomBubbleStudentView& operator=(const ClassroomBubbleStudentView&) =
      delete;
  ~ClassroomBubbleStudentView() override;

  // Handle switching between assignment lists.
  void SelectedAssignmentListChanged();

  // Handles received student assignments by rendering them in
  // `list_container_view_`.
  void OnGetStudentAssignments(
      std::vector<std::unique_ptr<GlanceablesClassroomStudentAssignment>>
          assignments);

  base::WeakPtrFactory<ClassroomBubbleStudentView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_STUDENT_VIEW_H_
