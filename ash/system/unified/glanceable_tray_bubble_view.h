// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_BUBBLE_VIEW_H_
#define ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_BUBBLE_VIEW_H_

#include "ash/system/tray/tray_bubble_view.h"

namespace views {
class Label;
}

namespace ash {
class ClassroomBubbleView;
class TasksBubbleView;
class Shelf;

// The bubble associated with the `GlanceableTrayBubble`. This bubble is the
// container for the child `tasks` and `classroom` glanceables.
class GlanceableTrayBubbleView : public TrayBubbleView {
 public:
  GlanceableTrayBubbleView(const InitParams& init_params, Shelf* shelf);
  GlanceableTrayBubbleView(const GlanceableTrayBubbleView&) = delete;
  GlanceableTrayBubbleView& operator=(const GlanceableTrayBubbleView&) = delete;
  ~GlanceableTrayBubbleView() override = default;

  void UpdateBubble();

  TasksBubbleView* GetTasksView() const { return tasks_bubble_view_; }

  // TrayBubbleView:
  bool CanActivate() const override;

 private:
  const raw_ptr<Shelf, ExperimentalAsh> shelf_;

  // Stand-in title label for glanceables_view_.
  // TODO(b:277268122): Remove and replace with actual glanceable content.
  raw_ptr<views::Label, ExperimentalAsh> title_label_ = nullptr;

  // A scrollable view which contains the individual glanceables.
  raw_ptr<views::ScrollView, ExperimentalAsh> scroll_view_ = nullptr;

  // Child bubble view for the tasks glanceable. Owned by bubble_view_.
  raw_ptr<TasksBubbleView, ExperimentalAsh> tasks_bubble_view_ = nullptr;

  // Child bubble view for the classrooms glanceable. Owned by bubble_view_.
  raw_ptr<ClassroomBubbleView, ExperimentalAsh> classroom_bubble_view_ =
      nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_BUBBLE_VIEW_H_
