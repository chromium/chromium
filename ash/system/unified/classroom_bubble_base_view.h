// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_BASE_VIEW_H_
#define ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_BASE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/unified/glanceable_tray_child_bubble.h"
#include "ui/base/metadata/metadata_header_macros.h"

class GURL;

namespace views {
class Combobox;
}

namespace ui {
class ComboboxModel;
}

namespace ash {

class GlanceablesListFooterView;
struct GlanceablesClassroomAssignment;

class ASH_EXPORT ClassroomBubbleBaseView : public GlanceableTrayChildBubble {
 public:
  METADATA_HEADER(ClassroomBubbleBaseView);

  // TODO(b:283370907): Add classroom glanceable contents.
  ClassroomBubbleBaseView(DetailedViewDelegate* delegate,
                          std::unique_ptr<ui::ComboboxModel> combobox_model);
  ClassroomBubbleBaseView(const ClassroomBubbleBaseView&) = delete;
  ClassroomBubbleBaseView& operator=(const ClassroomBubbleBaseView&) = delete;
  ~ClassroomBubbleBaseView() override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 protected:
  // Handles press on the "See all" button in `GlanceablesListFooterView`. Opens
  // classroom web UI based on the selected menu option.
  virtual void OnSeeAllPressed() = 0;

  // Handles received assignments by rendering them in `list_container_view_`.
  void OnGetAssignments(
      std::vector<std::unique_ptr<GlanceablesClassroomAssignment>> assignments);

  // Opens classroom url.
  void OpenUrl(const GURL& url) const;

  // Owned by views hierarchy.
  raw_ptr<views::FlexLayoutView, ExperimentalAsh> header_view_ = nullptr;
  raw_ptr<views::Combobox, ExperimentalAsh> combo_box_view_ = nullptr;
  raw_ptr<views::View, ExperimentalAsh> list_container_view_ = nullptr;
  raw_ptr<GlanceablesListFooterView, ExperimentalAsh> list_footer_view_ =
      nullptr;

  base::WeakPtrFactory<ClassroomBubbleBaseView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_CLASSROOM_BUBBLE_BASE_VIEW_H_
