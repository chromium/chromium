// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_BUBBLE_VIEW_H_
#define ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_BUBBLE_VIEW_H_

#include <memory>
#include <vector>

#include "ash/glanceables/classroom/glanceables_classroom_student_view.h"
#include "ash/glanceables/tasks/glanceables_tasks_view.h"
#include "ash/system/screen_layout_observer.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ui {
template <class ItemType>
class ListModel;
}

namespace views {
class View;
}  // namespace views

namespace ash {

namespace api {
struct TaskList;
}  // namespace api

class CalendarView;
class Shelf;
struct GlanceablesClassroomAssignment;

// The bubble associated with the `GlanceableTrayBubble`. This bubble is the
// container for the child `tasks` and `classroom` glanceables.
class GlanceableTrayBubbleView : public TrayBubbleView,
                                 public ScreenLayoutObserver {
  METADATA_HEADER(GlanceableTrayBubbleView, TrayBubbleView)

 public:
  GlanceableTrayBubbleView(const InitParams& init_params, Shelf* shelf);
  GlanceableTrayBubbleView(const GlanceableTrayBubbleView&) = delete;
  GlanceableTrayBubbleView& operator=(const GlanceableTrayBubbleView&) = delete;
  ~GlanceableTrayBubbleView() override;

  void InitializeContents();

  views::View* GetTasksView() { return tasks_bubble_view_; }
  views::View* GetClassroomStudentView() {
    return classroom_bubble_student_view_;
  }
  CalendarView* GetCalendarView() { return calendar_view_; }

  // views::View:
  int GetHeightForWidth(int w) const override;

  // TrayBubbleView:
  void AddedToWidget() override;
  void OnWidgetClosing(views::Widget* widget) override;

  // ScreenLayoutObserver:
  void OnDisplayConfigurationChanged() override;

 private:
  // Creates classroom student view if needed (if the corresponding
  // role is active).
  void AddClassroomBubbleStudentViewIfNeeded(bool is_role_active);
  void AddTaskBubbleViewIfNeeded(
      bool fetch_success,
      const ui::ListModel<api::TaskList>* task_lists);

  // Updates the cached task lists to `task_lists`.
  void UpdateTaskLists(bool fetch_success,
                       const ui::ListModel<api::TaskList>* task_lists);

  void OnGlanceablesContainerPreferredSizeChanged();
  void OnGlanceablesContainerHeightChanged(int height_delta);

  // Adjusts the order of the views in the focus list under
  // GlanceableTrayBubbleView.
  void AdjustChildrenFocusOrder();

  // Sets the preferred size of `calendar_view_`. This is called during
  // initialization and when the screen height changes.
  void SetCalendarPreferredSize() const;

  // For GlanceablesV2CalendarView: clips the `scroll_view_` height based on
  // `screen_max_height` and `calendar_view_` height. This is called during
  // initialization and when the `calendar_view_` height changes.
  void ClipScrollViewHeight(int screen_max_height) const;

  // Creates `time_management_container_view_` if needed.
  void MaybeCreateTimeManagementContainer();

  // Temporary method for `GlanceablesTimeManagementClassroomStudentData`
  // feature.
  void OnPotentialStudentAssignmentsLoaded(
      bool success,
      std::vector<std::unique_ptr<GlanceablesClassroomAssignment>> assignments)
      const;

  const raw_ptr<Shelf> shelf_;

  // Whether the bubble view has been initialized.
  bool initialized_ = false;

  // A scrollable view which contains the individual glanceables.
  raw_ptr<views::ScrollView> scroll_view_ = nullptr;

  // Container view for the tasks and classroom glanceables. Owned by this view.
  raw_ptr<views::FlexLayoutView> time_management_container_view_ = nullptr;

  // Child bubble view for the tasks glanceable. Owned by this view.
  raw_ptr<GlanceablesTasksView> tasks_bubble_view_ = nullptr;

  // Child bubble view for the student classrooms glanceable. Owned by
  // this view.
  raw_ptr<GlanceablesClassroomStudentView> classroom_bubble_student_view_ =
      nullptr;

  // The parent container of `calendar_view_`. Only exists if the glanceables
  // calendar flag is on.
  raw_ptr<views::FlexLayoutView> calendar_container_ = nullptr;

  // Child bubble view for the calendar glanceable. Owned by this view.
  raw_ptr<CalendarView> calendar_view_ = nullptr;

  base::CallbackListSubscription on_contents_scrolled_subscription_;

  base::WeakPtrFactory<GlanceableTrayBubbleView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_BUBBLE_VIEW_H_
