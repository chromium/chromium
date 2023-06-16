// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_VIEW_H_
#define ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "ash/system/unified/glanceable_tray_child_bubble.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class Combobox;
}

namespace ash {

class ASH_EXPORT ClassroomBubbleView : public GlanceableTrayChildBubble {
 public:
  METADATA_HEADER(ClassroomBubbleView);

  // TODO(b:283370907): Add classroom glanceable contents.
  ClassroomBubbleView();
  ClassroomBubbleView(const ClassroomBubbleView&) = delete;
  ClassroomBubbleView& operator-(const ClassroomBubbleView&) = delete;
  ~ClassroomBubbleView() override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  void OnGetStudentAssignmentsDueSoon(
      std::vector<std::unique_ptr<GlanceablesClassroomStudentAssignment>>
          assignments);

 private:
  // Handle switching between assignment lists.
  void SelectedAssignmentListChanged();

  // Owned by views hierarchy.
  raw_ptr<views::FlexLayoutView, ExperimentalAsh> header_view_ = nullptr;
  raw_ptr<views::Combobox, ExperimentalAsh> combo_box_view_ = nullptr;
  raw_ptr<views::FlexLayoutView, ExperimentalAsh> list_container_view_ =
      nullptr;

  base::WeakPtrFactory<ClassroomBubbleView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_VIEW_H_
