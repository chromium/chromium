// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/classroom/fake_glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_student_view.h"
#include "ash/glanceables/common/glanceables_contents_scroll_view.h"
#include "ash/glanceables/common/glanceables_time_management_bubble_view.h"
#include "ash/glanceables/common/glanceables_util.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/tasks/glanceables_tasks_view.h"
#include "ash/glanceables/tasks/test/glanceables_tasks_test_util.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "ash/style/counter_expand_button.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/date_tray.h"
#include "ash/system/unified/glanceable_tray_bubble.h"
#include "ash/system/unified/glanceable_tray_bubble_view.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/test/widget_animation_waiter.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

class ResizeAnimationWaiter {
 public:
  explicit ResizeAnimationWaiter(
      GlanceablesTimeManagementBubbleView* bubble_view)
      : bubble_view_(bubble_view) {}
  ResizeAnimationWaiter(const ResizeAnimationWaiter&) = delete;
  ResizeAnimationWaiter& operator=(const ResizeAnimationWaiter&) = delete;
  ~ResizeAnimationWaiter() = default;

  void Wait() {
    // Only run the `run_loop_` if the bubble is animating.
    if (bubble_view_->is_animating_resize()) {
      base::RunLoop run_loop;
      bubble_view_->SetAnimationEndedClosureForTest(run_loop.QuitClosure());
      run_loop.Run();
    }

    // Force frames and wait for all throughput trackers to be gone to allow
    // animation throughput data to be passed from cc to ui.
    ui::Compositor* compositor = bubble_view_->GetWidget()->GetCompositor();
    while (compositor->has_throughput_trackers_for_testing()) {
      compositor->ScheduleFullRedraw();
      std::ignore = ui::WaitForNextFrameToBePresented(compositor,
                                                      base::Milliseconds(500));
    }
  }

 private:
  raw_ptr<GlanceablesTimeManagementBubbleView> bubble_view_;
  base::WeakPtrFactory<ResizeAnimationWaiter> weak_ptr_factory_{this};
};

}  // namespace

class GlanceablesBaseTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    const auto account_id =
        AccountId::FromUserEmailGaiaId("test_user@gmail.com", "123456");
    SimulateUserLogin(account_id);

    classroom_client_ = std::make_unique<FakeGlanceablesClassroomClient>();
    tasks_client_ = glanceables_tasks_test_util::InitializeFakeTasksClient(
        base::Time::Now());
    tasks_client_->set_http_error(google_apis::ApiErrorCode::HTTP_SUCCESS);
    Shell::Get()->glanceables_controller()->UpdateClientsRegistration(
        account_id, GlanceablesController::ClientsRegistration{
                        .classroom_client = classroom_client_.get(),
                        .tasks_client = tasks_client_.get()});
    ASSERT_TRUE(Shell::Get()->glanceables_controller()->GetClassroomClient());
    ASSERT_TRUE(Shell::Get()->glanceables_controller()->GetTasksClient());
  }

  FakeGlanceablesClassroomClient* classroom_client() const {
    return classroom_client_.get();
  }
  api::FakeTasksClient* tasks_client() const { return tasks_client_.get(); }
  DateTray* date_tray() const {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()->date_tray();
  }

 private:
  std::unique_ptr<FakeGlanceablesClassroomClient> classroom_client_;
  std::unique_ptr<api::FakeTasksClient> tasks_client_;
};

TEST_F(GlanceablesBaseTest, DoesNotAddClassroomViewWhenDisabledByAdmin) {
  // Open Glanceables via Search + C, make sure the bubble is shown with the
  // Classroom view available.
  EXPECT_FALSE(date_tray()->is_active());
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));
  EXPECT_TRUE(date_tray()->is_active());
  EXPECT_TRUE(
      date_tray()->glanceables_bubble_for_test()->GetClassroomStudentView());

  // Close Glanceables.
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));

  // Simulate that admin disables the integration.
  classroom_client()->set_is_disabled_by_admin(true);

  // Open Glanceables via Search + C again, make sure the bubble no longer
  // contains the Classroom view.
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));
  EXPECT_TRUE(date_tray()->is_active());
  EXPECT_FALSE(
      date_tray()->glanceables_bubble_for_test()->GetClassroomStudentView());

  // Close Glanceables.
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));
}

TEST_F(GlanceablesBaseTest, DoesNotAddTasksViewWhenDisabledByAdmin) {
  // Open Glanceables via Search + C, make sure the bubble is shown with the
  // Tasks view available.
  EXPECT_FALSE(date_tray()->is_active());
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));
  EXPECT_TRUE(date_tray()->is_active());
  EXPECT_TRUE(date_tray()->glanceables_bubble_for_test()->GetTasksView());

  // Close Glanceables.
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));

  // Simulate that admin disables the integration.
  tasks_client()->set_is_disabled_by_admin(true);

  // Open Glanceables via Search + C again, make sure the bubble no longer
  // contains the Tasks view.
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));
  EXPECT_TRUE(date_tray()->is_active());
  EXPECT_FALSE(date_tray()->glanceables_bubble_for_test()->GetTasksView());

  // Close Glanceables.
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));
}

class GlanceablesTasksAndClassroomTest : public GlanceablesBaseTest {
 public:
  void SetUp() override {
    GlanceablesBaseTest::SetUp();

    date_tray()->ShowGlanceableBubble(/*from_keyboard=*/false);
    view_ = views::AsViewClass<GlanceableTrayBubbleView>(
        date_tray()->glanceables_bubble_for_test()->GetBubbleView());

    glanceables_util::SetIsNetworkConnectedForTest(true);
  }

  void TearDown() override {
    date_tray()->HideGlanceableBubble();
    view_ = nullptr;
    AshTestBase::TearDown();
  }

  void ReopenGlanceables() {
    date_tray()->HideGlanceableBubble();
    view_ = nullptr;
    date_tray()->ShowGlanceableBubble(/*from_keyboard=*/false);
    view_ = views::AsViewClass<GlanceableTrayBubbleView>(
        date_tray()->glanceables_bubble_for_test()->GetBubbleView());
  }

  // Populates `num` of tasks to the default task list.
  void PopulateTasks(size_t num) {
    for (size_t i = 0; i < num; ++i) {
      auto num_string = base::NumberToString(i);
      tasks_client()->AddTask("TaskListID1",
                              base::StrCat({"title_", num_string}),
                              base::DoNothing());
    }

    // Simulate closing the glanceables bubble to cache the tasks.
    tasks_client()->OnGlanceablesBubbleClosed(base::DoNothing());

    // Recreate the tasks view to update the task views.
    date_tray()->ShowGlanceableBubble(/*from_keyboard=*/false);
    view_ = views::AsViewClass<GlanceableTrayBubbleView>(
        date_tray()->glanceables_bubble_for_test()->GetBubbleView());
  }

  void GenerateGestureScrollEvent(gfx::Point gesture_position,
                                  bool upward,
                                  int distance_to_scroll) {
    // To move the scroll view contents upward, we need to scroll the gesture
    // downward.
    const gfx::Vector2d scroll_distance =
        upward ? gfx::Vector2d(0, distance_to_scroll)
               : gfx::Vector2d(0, -distance_to_scroll);
    GetEventGenerator()->GestureScrollSequence(
        gesture_position, gesture_position + scroll_distance,
        base::Milliseconds(100), /*steps=*/10);
  }

  void GenerateTrackpadScrollEvent(gfx::Point mouse_position,
                                   bool upward,
                                   int distance_to_scroll) {
    GetEventGenerator()->ScrollSequence(
        mouse_position, base::TimeDelta(), /*x_offset=*/0,
        upward ? distance_to_scroll : -distance_to_scroll, /*steps=*/1,
        /*num_fingers=*/2);
  }

  GlanceablesTasksView* GetTasksView() const {
    return views::AsViewClass<GlanceablesTasksView>(view_->GetTasksView());
  }

  CounterExpandButton* GetTasksExpandButtonView() const {
    return views::AsViewClass<CounterExpandButton>(
        GetTasksView()->GetViewByID(base::to_underlying(
            GlanceablesViewId::kTimeManagementBubbleExpandButton)));
  }

  views::ScrollView* GetTasksScrollView() const {
    return views::AsViewClass<views::ScrollView>(GetTasksView()->GetViewByID(
        base::to_underlying(GlanceablesViewId::kContentsScrollView)));
  }

  GlanceablesClassroomStudentView* GetClassroomView() const {
    return views::AsViewClass<GlanceablesClassroomStudentView>(
        view_->GetClassroomStudentView());
  }

  CounterExpandButton* GetClassroomExpandButtonView() const {
    return views::AsViewClass<CounterExpandButton>(
        GetClassroomView()->GetViewByID(base::to_underlying(
            GlanceablesViewId::kTimeManagementBubbleExpandButton)));
  }

  views::ScrollView* GetClassroomScrollView() const {
    return views::AsViewClass<views::ScrollView>(
        GetClassroomView()->GetViewByID(
            base::to_underlying(GlanceablesViewId::kContentsScrollView)));
  }

  GlanceableTrayBubbleView* view() const { return view_; }

 private:
  raw_ptr<GlanceableTrayBubbleView> view_ = nullptr;
};

TEST_F(GlanceablesTasksAndClassroomTest, Basics) {
  auto* const tasks_view = GetTasksView();
  EXPECT_TRUE(tasks_view);
  auto* const classroom_view = GetClassroomView();
  EXPECT_TRUE(classroom_view);

  // Check that both views have their own backgrounds.
  EXPECT_TRUE(tasks_view->GetBackground());
  EXPECT_TRUE(classroom_view->GetBackground());

  // Check that both views contain their expand buttons.
  EXPECT_TRUE(GetTasksExpandButtonView());
  EXPECT_TRUE(GetClassroomExpandButtonView());
}

TEST_F(GlanceablesTasksAndClassroomTest, TimeManagementExpandStates) {
  auto* const tasks_view = GetTasksView();
  auto* const classroom_view = GetClassroomView();

  // Initially `tasks_view` is expanded and `classroom_view` is collapsed.
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());

  // Verify the expand states of the buttons.
  auto* const tasks_expand_button = GetTasksExpandButtonView();
  auto* const classroom_expand_button = GetClassroomExpandButtonView();
  ASSERT_TRUE(tasks_expand_button);
  ASSERT_TRUE(classroom_expand_button);
  EXPECT_TRUE(tasks_expand_button->expanded());
  EXPECT_FALSE(classroom_expand_button->expanded());
  EXPECT_EQ(tasks_expand_button->GetTooltipText(), u"Collapse Google Tasks");
  EXPECT_EQ(classroom_expand_button->GetTooltipText(),
            u"Expand Google Classroom");

  // Expanding/Collapsing `tasks_view` will collapse/expand `classroom_view`.
  LeftClickOn(tasks_expand_button);
  EXPECT_FALSE(tasks_view->IsExpanded());
  EXPECT_TRUE(classroom_view->IsExpanded());
  EXPECT_EQ(tasks_expand_button->GetTooltipText(), u"Expand Google Tasks");
  EXPECT_EQ(classroom_expand_button->GetTooltipText(),
            u"Collapse Google Classroom");

  LeftClickOn(tasks_expand_button);
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());
  EXPECT_EQ(tasks_expand_button->GetTooltipText(), u"Collapse Google Tasks");
  EXPECT_EQ(classroom_expand_button->GetTooltipText(),
            u"Expand Google Classroom");

  // Same for `classroom_view`.
  LeftClickOn(classroom_expand_button);
  EXPECT_FALSE(tasks_view->IsExpanded());
  EXPECT_TRUE(classroom_view->IsExpanded());
  EXPECT_EQ(tasks_expand_button->GetTooltipText(), u"Expand Google Tasks");
  EXPECT_EQ(classroom_expand_button->GetTooltipText(),
            u"Collapse Google Classroom");

  LeftClickOn(classroom_expand_button);
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());
  EXPECT_EQ(tasks_expand_button->GetTooltipText(), u"Collapse Google Tasks");
  EXPECT_EQ(classroom_expand_button->GetTooltipText(),
            u"Expand Google Classroom");
}

TEST_F(GlanceablesTasksAndClassroomTest,
       TrackpadScrollingDownFromTheBottomOfTasksExpandsClassroom) {
  // Increase the number of tasks to ensure the scroll contents overflow.
  PopulateTasks(10);

  auto* const tasks_view = GetTasksView();
  auto* const classroom_view = GetClassroomView();
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());

  view()->GetWidget()->LayoutRootViewIfNecessary();

  // Make sure the scroll view is scrollable.
  auto* const tasks_scroll_bar = GetTasksScrollView()->vertical_scroll_bar();
  EXPECT_TRUE(tasks_scroll_bar->GetVisible());
  const gfx::Point tasks_scroll_view_center =
      GetTasksScrollView()->GetBoundsInScreen().CenterPoint();

  // Set the distance that we want to scroll to the amount that is greater than
  // the scrollable length of the scroll view.
  const int distance_to_scroll = tasks_scroll_bar->GetMaxPosition() -
                                 tasks_scroll_bar->GetMinPosition() + 10;

  // Scrolling upward at the top of the scroll view doesn't change expand state.
  GenerateTrackpadScrollEvent(tasks_scroll_view_center, /*upward=*/true,
                              distance_to_scroll);
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());

  // Scrolling downward when there is scrollable content doesn't change expand
  // state.
  GenerateTrackpadScrollEvent(tasks_scroll_view_center, /*upward=*/false,
                              distance_to_scroll);
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());

  // Scrolling downward at the bottom of the scroll view changes expand state.
  GenerateTrackpadScrollEvent(tasks_scroll_view_center, /*upward=*/false,
                              distance_to_scroll);
  EXPECT_FALSE(tasks_view->IsExpanded());
  EXPECT_TRUE(classroom_view->IsExpanded());
}

TEST_F(GlanceablesTasksAndClassroomTest,
       GestureScrollingDownFromTheBottomOfTasksExpandsClassroom) {
  // Increase the number of tasks to ensure the scroll contents overflow.
  PopulateTasks(10);

  auto* const tasks_view = GetTasksView();
  auto* const classroom_view = GetClassroomView();
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());

  view()->GetWidget()->LayoutRootViewIfNecessary();

  // Make sure the scroll view is scrollable.
  auto* const tasks_scroll_bar = GetTasksScrollView()->vertical_scroll_bar();
  EXPECT_TRUE(tasks_scroll_bar->GetVisible());
  const gfx::Point tasks_scroll_view_center =
      GetTasksScrollView()->GetBoundsInScreen().CenterPoint();

  // Set the distance that we want to scroll to the amount that is greater than
  // the scrollable length of the scroll view.
  const int distance_to_scroll = tasks_scroll_bar->GetMaxPosition() -
                                 tasks_scroll_bar->GetMinPosition() + 10;

  // Scrolling upward at the top of the scroll view doesn't change expand state.
  GenerateGestureScrollEvent(tasks_scroll_view_center, /*upward=*/true,
                             distance_to_scroll);
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());

  // Scrolling downward when there is scrollable content doesn't change expand
  // state.
  GenerateGestureScrollEvent(tasks_scroll_view_center, /*upward=*/false,
                             distance_to_scroll);
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());

  // Scrolling downward at the bottom of the scroll view changes expand state.
  GenerateGestureScrollEvent(tasks_scroll_view_center, /*upward=*/false,
                             distance_to_scroll);
  EXPECT_FALSE(tasks_view->IsExpanded());
  EXPECT_TRUE(classroom_view->IsExpanded());
}

TEST_F(GlanceablesTasksAndClassroomTest,
       MouseWheelScrollingDownFromTheBottomOfTasksExpandsClassroom) {
  // Increase the number of tasks to ensure the scroll contents overflow.
  PopulateTasks(10);

  auto* const tasks_view = GetTasksView();
  auto* const classroom_view = GetClassroomView();
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());

  view()->GetWidget()->LayoutRootViewIfNecessary();

  // Make sure the scroll view is scrollable.
  auto* const tasks_scroll_bar = GetTasksScrollView()->vertical_scroll_bar();
  EXPECT_TRUE(tasks_scroll_bar->GetVisible());
  const gfx::Point tasks_scroll_view_center =
      GetTasksScrollView()->GetBoundsInScreen().CenterPoint();

  // Set the distance that we want to scroll to the amount that is greater than
  // the scrollable length of the scroll view.
  const int distance_to_scroll = tasks_scroll_bar->GetMaxPosition() -
                                 tasks_scroll_bar->GetMinPosition() + 10;

  // Scrolling upward at the top of the scroll view doesn't change expand state.
  GetEventGenerator()->MoveMouseTo(tasks_scroll_view_center);
  GetEventGenerator()->MoveMouseWheel(0, distance_to_scroll);
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());

  // Scrolling downward when there is scrollable content doesn't change expand
  // state.
  GetEventGenerator()->MoveMouseWheel(0, -distance_to_scroll);
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());

  // Right after hitting the bottom of the scroll view, scrolling downward at
  // the bottom of the scroll view doesn't change expand state.
  GetEventGenerator()->MoveMouseWheel(0, -distance_to_scroll);
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());

  // After the mouse wheel is fired, scrolling downward at the bottom of the
  // scroll view changes expand state.
  views::AsViewClass<GlanceablesContentsScrollView>(GetTasksScrollView())
      ->FireMouseWheelTimerForTest();
  GetEventGenerator()->MoveMouseWheel(0, -distance_to_scroll);
  EXPECT_FALSE(tasks_view->IsExpanded());
  EXPECT_TRUE(classroom_view->IsExpanded());
}

TEST_F(GlanceablesTasksAndClassroomTest,
       TrackpadScrollingUpFromTheTopOfClassroomExpandsTasks) {
  // Expand classroom first to make the scroll view visible.
  auto const* classroom_expand_button = GetClassroomExpandButtonView();
  ASSERT_TRUE(classroom_expand_button);
  LeftClickOn(classroom_expand_button);
  auto* const tasks_view = GetTasksView();
  auto* const classroom_view = GetClassroomView();
  EXPECT_FALSE(tasks_view->IsExpanded());
  EXPECT_TRUE(classroom_view->IsExpanded());

  view()->GetWidget()->LayoutRootViewIfNecessary();

  // Make sure the scroll view is scrollable.
  auto* const classroom_scroll_bar =
      GetClassroomScrollView()->vertical_scroll_bar();
  EXPECT_TRUE(classroom_scroll_bar->GetVisible());
  const gfx::Point classroom_scroll_view_center =
      GetClassroomScrollView()->GetBoundsInScreen().CenterPoint();

  // Set the distance that we want to scroll to the amount that is greater than
  // the scrollable length of the scroll view.
  const int distance_to_scroll = classroom_scroll_bar->GetMaxPosition() -
                                 classroom_scroll_bar->GetMinPosition() + 10;

  // Scrolling downward to the bottom of the scroll view doesn't change expand
  // state.
  GenerateTrackpadScrollEvent(classroom_scroll_view_center, /*upward=*/false,
                              distance_to_scroll);
  EXPECT_FALSE(tasks_view->IsExpanded());
  EXPECT_TRUE(classroom_view->IsExpanded());

  // Scrolling upward when there is scrollable content doesn't change expand
  // state.
  GenerateTrackpadScrollEvent(classroom_scroll_view_center, /*upward=*/true,
                              distance_to_scroll);
  EXPECT_FALSE(tasks_view->IsExpanded());
  EXPECT_TRUE(classroom_view->IsExpanded());

  // Scrolling upward at the top of the scroll view changes expand state.
  GenerateTrackpadScrollEvent(classroom_scroll_view_center, /*upward=*/true,
                              distance_to_scroll);
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());
}

TEST_F(GlanceablesTasksAndClassroomTest,
       GestureScrollingUpFromTheTopOfClassroomExpandsTasks) {
  // Expand classroom first to make the scroll view visible.
  auto const* classroom_expand_button = GetClassroomExpandButtonView();
  ASSERT_TRUE(classroom_expand_button);
  LeftClickOn(classroom_expand_button);
  auto* const tasks_view = GetTasksView();
  auto* const classroom_view = GetClassroomView();
  EXPECT_FALSE(tasks_view->IsExpanded());
  EXPECT_TRUE(classroom_view->IsExpanded());

  view()->GetWidget()->LayoutRootViewIfNecessary();

  // Make sure the scroll view is scrollable.
  auto* const classroom_scroll_bar =
      GetClassroomScrollView()->vertical_scroll_bar();
  EXPECT_TRUE(classroom_scroll_bar->GetVisible());
  const gfx::Point classroom_scroll_view_center =
      GetClassroomScrollView()->GetBoundsInScreen().CenterPoint();

  // Set the distance that we want to scroll to the amount that is greater than
  // the scrollable length of the scroll view.
  const int distance_to_scroll = classroom_scroll_bar->GetMaxPosition() -
                                 classroom_scroll_bar->GetMinPosition() + 10;

  // Scrolling downward to the bottom of the scroll view doesn't change expand
  // state.
  GenerateGestureScrollEvent(classroom_scroll_view_center, /*upward=*/false,
                             distance_to_scroll);
  EXPECT_FALSE(tasks_view->IsExpanded());
  EXPECT_TRUE(classroom_view->IsExpanded());

  // Scrolling upward when there is scrollable content doesn't change expand
  // state.
  GenerateGestureScrollEvent(classroom_scroll_view_center, /*upward=*/true,
                             distance_to_scroll);
  EXPECT_FALSE(tasks_view->IsExpanded());
  EXPECT_TRUE(classroom_view->IsExpanded());

  // Scrolling upward at the top of the scroll view changes expand state.
  GenerateGestureScrollEvent(classroom_scroll_view_center, /*upward=*/true,
                             distance_to_scroll);
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());
}

TEST_F(GlanceablesTasksAndClassroomTest,
       MouseWheelScrollingUpFromTheTopOfClassroomExpandsTasks) {
  // Expand classroom first to make the scroll view visible.
  auto const* classroom_expand_button = GetClassroomExpandButtonView();
  ASSERT_TRUE(classroom_expand_button);
  LeftClickOn(classroom_expand_button);
  auto* const tasks_view = GetTasksView();
  auto* const classroom_view = GetClassroomView();
  EXPECT_FALSE(tasks_view->IsExpanded());
  EXPECT_TRUE(classroom_view->IsExpanded());

  view()->GetWidget()->LayoutRootViewIfNecessary();

  // Make sure the scroll view is scrollable.
  auto* const classroom_scroll_bar =
      GetClassroomScrollView()->vertical_scroll_bar();
  EXPECT_TRUE(classroom_scroll_bar->GetVisible());
  const gfx::Point classroom_scroll_view_center =
      GetClassroomScrollView()->GetBoundsInScreen().CenterPoint();

  // Set the distance that we want to scroll to the amount that is greater than
  // the scrollable length of the scroll view.
  const int distance_to_scroll = classroom_scroll_bar->GetMaxPosition() -
                                 classroom_scroll_bar->GetMinPosition() + 10;

  // Scrolling downward to the bottom of the scroll view doesn't change expand
  // state.
  GetEventGenerator()->MoveMouseTo(classroom_scroll_view_center);
  GetEventGenerator()->MoveMouseWheel(0, -distance_to_scroll);
  EXPECT_FALSE(tasks_view->IsExpanded());
  EXPECT_TRUE(classroom_view->IsExpanded());

  // Scrolling upward when there is scrollable content doesn't change expand
  // state.
  GetEventGenerator()->MoveMouseWheel(0, distance_to_scroll);
  EXPECT_FALSE(tasks_view->IsExpanded());
  EXPECT_TRUE(classroom_view->IsExpanded());

  // Right after hitting the top of the scroll view, scrolling upward at the top
  // of the scroll view doesn't change expand state.
  GetEventGenerator()->MoveMouseWheel(0, distance_to_scroll);
  EXPECT_FALSE(tasks_view->IsExpanded());
  EXPECT_TRUE(classroom_view->IsExpanded());

  // After the mouse wheel timer is fired, scrolling upward at the top of the
  // scroll view changes expand state.
  views::AsViewClass<GlanceablesContentsScrollView>(GetClassroomScrollView())
      ->FireMouseWheelTimerForTest();
  GetEventGenerator()->MoveMouseWheel(0, distance_to_scroll);
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());
}

TEST_F(GlanceablesTasksAndClassroomTest, ScrollLockAfterOverscroll) {
  // Increase the number of tasks and assignments to ensure the scroll contents
  // overflow.
  classroom_client()->SetAssignmentsCount(10);
  PopulateTasks(10);

  auto* const tasks_view = GetTasksView();
  auto* const classroom_view = GetClassroomView();
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());

  view()->GetWidget()->LayoutRootViewIfNecessary();

  // Make sure the scroll view is scrollable.
  auto* const tasks_scroll_bar = GetTasksScrollView()->vertical_scroll_bar();
  EXPECT_TRUE(tasks_scroll_bar->GetVisible());
  const gfx::Point tasks_scroll_view_center =
      GetTasksScrollView()->GetBoundsInScreen().CenterPoint();

  // Set the distance that we want to scroll to the amount that is greater than
  // the scrollable length of the scroll view.
  const int distance_to_scroll = tasks_scroll_bar->GetMaxPosition() -
                                 tasks_scroll_bar->GetMinPosition() + 30;

  // Expand Classroom by scrolling.
  GenerateTrackpadScrollEvent(tasks_scroll_view_center, /*upward=*/false,
                              distance_to_scroll);
  const int tasks_scroll_bar_end_pos = tasks_scroll_bar->GetPosition();
  GenerateTrackpadScrollEvent(tasks_scroll_view_center, /*upward=*/false,
                              distance_to_scroll);
  EXPECT_FALSE(tasks_view->IsExpanded());
  EXPECT_TRUE(classroom_view->IsExpanded());

  view()->GetWidget()->LayoutRootViewIfNecessary();

  // Make sure the classroom scroll view stays at its min position after
  // overscroll.
  const auto* const classroom_scroll_bar =
      GetClassroomScrollView()->vertical_scroll_bar();
  EXPECT_TRUE(classroom_scroll_bar->GetVisible());
  EXPECT_EQ(classroom_scroll_bar->GetPosition(),
            classroom_scroll_bar->GetMinPosition());

  // After the scroll lock timer fires, the scroll view can scroll as usual.
  const gfx::Point classroom_scroll_view_center =
      GetClassroomScrollView()->GetBoundsInScreen().CenterPoint();
  views::AsViewClass<GlanceablesContentsScrollView>(GetClassroomScrollView())
      ->FireScrollLockTimerForTest();
  GenerateTrackpadScrollEvent(classroom_scroll_view_center, /*upward=*/false,
                              distance_to_scroll);
  EXPECT_GT(classroom_scroll_bar->GetPosition(),
            classroom_scroll_bar->GetMinPosition());

  // Expand Tasks by scrolling.
  GenerateTrackpadScrollEvent(classroom_scroll_view_center, /*upward=*/true,
                              distance_to_scroll);
  GenerateTrackpadScrollEvent(classroom_scroll_view_center, /*upward=*/true,
                              distance_to_scroll);
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());
  // Make sure the tasks scroll view stays at its max position after overscroll.
  EXPECT_EQ(tasks_scroll_bar->GetPosition(), tasks_scroll_bar_end_pos);

  // After the scroll lock timer fires, the scroll view can scroll as usual.
  views::AsViewClass<GlanceablesContentsScrollView>(GetTasksScrollView())
      ->FireScrollLockTimerForTest();
  GenerateTrackpadScrollEvent(tasks_scroll_view_center, /*upward=*/false,
                              distance_to_scroll);
  EXPECT_LT(classroom_scroll_bar->GetPosition(), tasks_scroll_bar_end_pos);
}

TEST_F(GlanceablesTasksAndClassroomTest,
       NonScrollableGlanceablesCanStillScrollToToggleExpand) {
  // Set a bigger display to fit classroom items.
  UpdateDisplay("1920x1080");

  auto* const tasks_view = GetTasksView();
  auto* const classroom_view = GetClassroomView();
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());

  view()->GetWidget()->LayoutRootViewIfNecessary();

  // Make sure the tasks scroll view is not scrollable.
  auto* const tasks_scroll_bar = GetTasksScrollView()->vertical_scroll_bar();
  EXPECT_FALSE(tasks_scroll_bar->GetVisible());
  const gfx::Point tasks_scroll_view_center =
      GetTasksScrollView()->GetBoundsInScreen().CenterPoint();

  const int distance_to_scroll = 10;

  // Scrolling downward when tasks scroll view is not scrollable expands
  // Classroom.
  GenerateGestureScrollEvent(tasks_scroll_view_center, /*upward=*/false,
                             distance_to_scroll);
  EXPECT_FALSE(tasks_view->IsExpanded());
  EXPECT_TRUE(classroom_view->IsExpanded());

  // Make sure the classroom scroll view is not scrollable.
  auto* const classroom_scroll_bar =
      GetClassroomScrollView()->vertical_scroll_bar();
  EXPECT_FALSE(classroom_scroll_bar->GetVisible());
  const gfx::Point classroom_scroll_view_center =
      GetClassroomScrollView()->GetBoundsInScreen().CenterPoint();

  // Scrolling upward when classroom scroll view is not scrollable expands
  // Tasks.
  GenerateGestureScrollEvent(classroom_scroll_view_center, /*upward=*/true,
                             distance_to_scroll);
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());

  // Scrolling downward when tasks scroll view is not scrollable expands
  // Classroom.
  GenerateTrackpadScrollEvent(tasks_scroll_view_center, /*upward=*/false,
                              distance_to_scroll);
  EXPECT_FALSE(tasks_view->IsExpanded());
  EXPECT_TRUE(classroom_view->IsExpanded());

  // Scrolling upward when classroom scroll view is not scrollable expands
  // Tasks.
  GenerateTrackpadScrollEvent(classroom_scroll_view_center, /*upward=*/true,
                              distance_to_scroll);
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());

  // Scrolling downward when tasks scroll view is not scrollable expands
  // Classroom.
  GetEventGenerator()->MoveMouseTo(tasks_scroll_view_center);
  GetEventGenerator()->MoveMouseWheel(0, -distance_to_scroll);
  EXPECT_FALSE(tasks_view->IsExpanded());
  EXPECT_TRUE(classroom_view->IsExpanded());

  // Scrolling upward when classroom scroll view is not scrollable expands
  // Tasks.
  GetEventGenerator()->MoveMouseTo(classroom_scroll_view_center);
  GetEventGenerator()->MoveMouseWheel(0, distance_to_scroll);
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());
}

TEST_F(GlanceablesTasksAndClassroomTest,
       ExpandCollapseAnimationSmoothnessHistogram) {
  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  base::HistogramTester resize_animation_histograms;

  // Smoothness should be recorded.
  resize_animation_histograms.ExpectTotalCount(
      "Ash.Glanceables.TimeManagement.Tasks.Expand.AnimationSmoothness", 0);
  resize_animation_histograms.ExpectTotalCount(
      "Ash.Glanceables.TimeManagement.Tasks.Collapse.AnimationSmoothness", 0);
  resize_animation_histograms.ExpectTotalCount(
      "Ash.Glanceables.TimeManagement.Classroom.Expand.AnimationSmoothness", 0);
  resize_animation_histograms.ExpectTotalCount(
      "Ash.Glanceables.TimeManagement.Classroom.Collapse.AnimationSmoothness",
      0);

  auto* const tasks_view = GetTasksView();
  auto* const classroom_view = GetClassroomView();

  auto* const tasks_expand_button = GetTasksExpandButtonView();
  ASSERT_TRUE(tasks_expand_button);
  LeftClickOn(tasks_expand_button);
  // Make sure both view animations ended.
  ResizeAnimationWaiter(tasks_view).Wait();
  ResizeAnimationWaiter(classroom_view).Wait();
  EXPECT_FALSE(tasks_view->IsExpanded());
  EXPECT_TRUE(classroom_view->IsExpanded());

  resize_animation_histograms.ExpectTotalCount(
      "Ash.Glanceables.TimeManagement.Tasks.Expand.AnimationSmoothness", 0);
  resize_animation_histograms.ExpectTotalCount(
      "Ash.Glanceables.TimeManagement.Tasks.Collapse.AnimationSmoothness", 1);
  resize_animation_histograms.ExpectTotalCount(
      "Ash.Glanceables.TimeManagement.Classroom.Expand.AnimationSmoothness", 1);
  resize_animation_histograms.ExpectTotalCount(
      "Ash.Glanceables.TimeManagement.Classroom.Collapse.AnimationSmoothness",
      0);

  auto const* classroom_expand_button = GetClassroomExpandButtonView();
  ASSERT_TRUE(classroom_expand_button);
  LeftClickOn(classroom_expand_button);
  // Make sure both view animations ended.
  ResizeAnimationWaiter(tasks_view).Wait();
  ResizeAnimationWaiter(classroom_view).Wait();
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());

  resize_animation_histograms.ExpectTotalCount(
      "Ash.Glanceables.TimeManagement.Tasks.Expand.AnimationSmoothness", 1);
  resize_animation_histograms.ExpectTotalCount(
      "Ash.Glanceables.TimeManagement.Tasks.Collapse.AnimationSmoothness", 1);
  resize_animation_histograms.ExpectTotalCount(
      "Ash.Glanceables.TimeManagement.Classroom.Expand.AnimationSmoothness", 1);
  resize_animation_histograms.ExpectTotalCount(
      "Ash.Glanceables.TimeManagement.Classroom.Collapse.AnimationSmoothness",
      1);
}

TEST_F(GlanceablesTasksAndClassroomTest,
       PrefsRememberWhatUsersExpandedLastTime) {
  // Tasks should be expanded by default.
  auto* tasks_view = GetTasksView();
  auto* classroom_view = GetClassroomView();
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());

  view()->GetWidget()->LayoutRootViewIfNecessary();

  // Expand classroom.
  auto const* classroom_expand_button = GetClassroomExpandButtonView();
  ASSERT_TRUE(classroom_expand_button);
  LeftClickOn(classroom_expand_button);
  EXPECT_FALSE(tasks_view->IsExpanded());
  EXPECT_TRUE(classroom_view->IsExpanded());

  // Reopen the glanceables again.
  ReopenGlanceables();
  tasks_view = GetTasksView();
  classroom_view = GetClassroomView();

  // Classroom should be expanded as this is expanded when the glanceables was
  // closed last time.
  EXPECT_FALSE(tasks_view->IsExpanded());
  EXPECT_TRUE(classroom_view->IsExpanded());

  // Expand Tasks.
  auto const* tasks_expand_button = GetTasksExpandButtonView();
  LeftClickOn(tasks_expand_button);
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());

  // Reopen the glanceables again.
  ReopenGlanceables();
  tasks_view = GetTasksView();
  classroom_view = GetClassroomView();

  // Tasks should be expanded as this is expanded when the glanceables was
  // closed last time.
  EXPECT_TRUE(tasks_view->IsExpanded());
  EXPECT_FALSE(classroom_view->IsExpanded());
}

}  // namespace ash
