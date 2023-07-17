// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_STUDENT_VIEW_H_
#define ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_STUDENT_VIEW_H_

#include "ash/system/unified/classroom_bubble_base_view.h"

namespace ash {

// class ClassroomBubbleStudentView : public views::View {
class ASH_EXPORT ClassroomBubbleStudentView : public ClassroomBubbleBaseView {
 public:
  METADATA_HEADER(ClassroomBubbleStudentView);

  explicit ClassroomBubbleStudentView(DetailedViewDelegate* delegate);
  ClassroomBubbleStudentView(const ClassroomBubbleStudentView&) = delete;
  ClassroomBubbleStudentView& operator=(const ClassroomBubbleStudentView&) =
      delete;
  ~ClassroomBubbleStudentView() override;

 private:
  // ClassroomBubbleBaseView:
  void OnSeeAllPressed() override;

  // Handle switching between assignment lists.
  void SelectedAssignmentListChanged();

  base::WeakPtrFactory<ClassroomBubbleStudentView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_STUDENT_VIEW_H_
