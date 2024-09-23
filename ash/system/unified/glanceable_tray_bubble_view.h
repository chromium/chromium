// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_BUBBLE_VIEW_H_
#define ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_BUBBLE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/glanceables/classroom/glanceables_classroom_student_view.h"
#include "ash/glanceables/tasks/glanceables_tasks_view.h"
#include "ash/system/screen_layout_observer.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/common/api_error_codes.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/layout_types.h"

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

// The bubble associated with the `GlanceableTrayBubble`. This bubble is the
// container for the child `tasks` and `classroom` glanceables.
class ASH_EXPORT GlanceableTrayBubbleView
    : public TrayBubbleView,
      public ScreenLayoutObserver,
      public GlanceablesTimeManagementBubbleView::Observer {
  METADATA_HEADER(GlanceableTrayBubbleView, TrayBubbleView)

 public:
  // Registers syncable user profile prefs with the specified `registry`.
  static void RegisterUserProfilePrefs(PrefRegistrySimple* registry);

  // Clears any glanceables tray bubble related state from user `pref_services`.
  static void ClearUserStatePrefs(PrefService* pref_service);

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
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  views::SizeBounds GetAvailableSize(const View* child) const override;

  // TrayBubbleView:
  void AddedToWidget() override;
  void OnWidgetClosing(views::Widget* widget) override;

  // ScreenLayoutObserver:
  void OnDidApplyDisplayChanges() override;

  // GlanceablesTimeManagementBubbleView::Observer:
  void OnExpandStateChanged(
      GlanceablesTimeManagementBubbleView::Context context,
      bool is_expanded,
      bool expand_by_overscroll) override;

 private:
  // Creates classroom student view if needed (if the corresponding
  // role is active).
  void AddClassroomBubbleStudentViewIfNeeded(bool is_role_active);
  void AddTaskBubbleViewIfNeeded(
      bool fetch_success,
      std::optional<google_apis::ApiErrorCode> http_error,
      const ui::ListModel<api::TaskList>* task_lists);

  // Sets the initial expand states of the child bubbles, which are Tasks and
  // Classroom.
  void UpdateChildBubblesInitialExpandState();

  // Updates the cached task lists to `task_lists`.
  void UpdateTaskLists(bool fetch_success,
                       std::optional<google_apis::ApiErrorCode> http_error,
                       const ui::ListModel<api::TaskList>* task_lists);

  // Adjusts the order of the views in the focus list under
  // GlanceableTrayBubbleView.
  void AdjustChildrenFocusOrder();

  // Sets the preferred size of `calendar_view_`. This is called during
  // initialization and when the screen height changes.
  void SetCalendarPreferredSize() const;

  // Creates `time_management_container_view_` if needed.
  void MaybeCreateTimeManagementContainer();

  // Updates `time_management_container_view_` layout according to the number of
  // its children.
  void UpdateTimeManagementContainerLayout();

  const raw_ptr<Shelf> shelf_;

  // Whether the bubble view has been initialized.
  bool initialized_ = false;

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

  base::ScopedMultiSourceObservation<
      GlanceablesTimeManagementBubbleView,
      GlanceablesTimeManagementBubbleView::Observer>
      time_management_view_observation_{this};

  base::WeakPtrFactory<GlanceableTrayBubbleView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_BUBBLE_VIEW_H_
