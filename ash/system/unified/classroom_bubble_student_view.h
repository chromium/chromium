// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_STUDENT_VIEW_H_
#define ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_STUDENT_VIEW_H_

#include "ash/system/unified/classroom_bubble_base_view.h"

class PrefRegistrySimple;
class PrefService;

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

  // Registers syncable user profile prefs with the specified `registry`.
  static void RegisterUserProfilePrefs(PrefRegistrySimple* registry);

  // Clears any student glanceables state from user `pref_services`.
  static void ClearUserStatePrefs(PrefService* pref_service);

 private:
  // ClassroomBubbleBaseView:
  void OnSeeAllPressed() override;

  // Handle switching between assignment lists.
  void SelectedAssignmentListChanged(bool initial_update);

  base::WeakPtrFactory<ClassroomBubbleStudentView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_STUDENT_VIEW_H_
