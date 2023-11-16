// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_BUBBLE_VIEW_H_
#define ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_BUBBLE_VIEW_H_

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
class ClassroomBubbleStudentView;
class ClassroomBubbleTeacherView;
class Shelf;

// The bubble associated with the `GlanceableTrayBubble`. This bubble is the
// container for the child `tasks` and `classroom` glanceables.
class GlanceableTrayBubbleView : public TrayBubbleView,
                                 public ScreenLayoutObserver {
 public:
  METADATA_HEADER(GlanceableTrayBubbleView);
  GlanceableTrayBubbleView(const InitParams& init_params, Shelf* shelf);
  GlanceableTrayBubbleView(const GlanceableTrayBubbleView&) = delete;
  GlanceableTrayBubbleView& operator=(const GlanceableTrayBubbleView&) = delete;
  ~GlanceableTrayBubbleView() override;

  void InitializeContents();

  views::View* GetTasksView() { return tasks_bubble_view_; }
  ClassroomBubbleTeacherView* GetClassroomTeacherView() {
    return classroom_bubble_teacher_view_;
  }
  ClassroomBubbleStudentView* GetClassroomStudentView() {
    return classroom_bubble_student_view_;
  }
  CalendarView* GetCalendarView() { return calendar_view_; }

  // TrayBubbleView:
  void AddedToWidget() override;
  void OnWidgetClosing(views::Widget* widget) override;

  // ScreenLayoutObserver:
  void OnDisplayConfigurationChanged() override;

 private:
  // Creates classroom student or teacher view if needed (if the corresponding
  // role is active) and stores the pointer in `view`.
  // NOTE: in the rare case, when a single user has both student and teacher
  // roles in different courses, the order of the two bubbles is not guaranteed.
  template <typename T>
  void AddClassroomBubbleViewIfNeeded(raw_ptr<T, ExperimentalAsh>* view,
                                      bool is_role_active);
  void AddTaskBubbleViewIfNeeded(
      const ui::ListModel<api::TaskList>* task_lists);

  void OnGlanceablesContainerPreferredSizeChanged();
  void OnGlanceablesContainerHeightChanged(int height_delta);

  const raw_ptr<Shelf, ExperimentalAsh> shelf_;

  // Whether the bubble view has been initialized.
  bool initialized_ = false;

  // A scrollable view which contains the individual glanceables.
  raw_ptr<views::ScrollView, ExperimentalAsh> scroll_view_ = nullptr;

  // Child bubble view for the tasks glanceable. Owned by bubble_view_.
  raw_ptr<GlanceablesTasksViewBase, ExperimentalAsh> tasks_bubble_view_ =
      nullptr;

  // Child bubble view for the teacher classrooms glanceable. Owned by
  // bubble_view_.
  raw_ptr<ClassroomBubbleTeacherView, ExperimentalAsh>
      classroom_bubble_teacher_view_ = nullptr;

  // Child bubble view for the student classrooms glanceable. Owned by
  // bubble_view_.
  raw_ptr<ClassroomBubbleStudentView, ExperimentalAsh>
      classroom_bubble_student_view_ = nullptr;

  // Child bubble view for the calendar glanceable. Owned by bubble_view_.
  raw_ptr<CalendarView, ExperimentalAsh> calendar_view_ = nullptr;

  base::CallbackListSubscription on_contents_scrolled_subscription_;

  base::WeakPtrFactory<GlanceableTrayBubbleView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_BUBBLE_VIEW_H_
