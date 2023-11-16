// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/api/tasks/fake_tasks_client.h"
#include "ash/constants/ash_features.h"
#include "ash/glanceables/common/glanceables_list_footer_view.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/common/test/glanceables_test_new_window_delegate.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/tasks/glanceables_task_view.h"
#include "ash/shell.h"
#include "ash/style/combobox.h"
#include "ash/style/icon_button.h"
#include "ash/system/unified/tasks_bubble_view.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

void WaitForTimeBetweenButtonOnClicks() {
  base::RunLoop loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), views::kMinimumTimeBetweenButtonClicks);
  loop.Run();
}

}  // namespace

class TasksBubbleViewTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();
    SimulateUserLogin(account_id_);
    fake_glanceables_tasks_client_ =
        std::make_unique<api::FakeTasksClient>(base::Time::Now());
    Shell::Get()->glanceables_controller()->UpdateClientsRegistration(
        account_id_, GlanceablesController::ClientsRegistration{
                         .tasks_client = fake_glanceables_tasks_client_.get()});
    ASSERT_TRUE(Shell::Get()->glanceables_controller()->GetTasksClient());

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);

    view_ = widget_->SetContentsView(std::make_unique<TasksBubbleView>(
        fake_glanceables_tasks_client_->task_lists()));
  }

  void TearDown() override {
    // Destroy `widget_` first, before destroying `LayoutProvider` (needed in
    // the `views::Combobox`'s destruction chain).
    view_ = nullptr;
    widget_.reset();
    AshTestBase::TearDown();
  }

  Combobox* GetComboBoxView() const {
    return views::AsViewClass<Combobox>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kTasksBubbleComboBox)));
  }

  bool IsMenuRunning() const {
    const auto* const combo_box = GetComboBoxView();
    return combo_box && combo_box->IsMenuRunning();
  }

  const views::View* GetTaskItemsContainerView() const {
    return views::AsViewClass<views::View>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kTasksBubbleListContainer)));
  }

  const views::LabelButton* GetAddNewTaskButton() const {
    return views::AsViewClass<views::LabelButton>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kTasksBubbleAddNewButton)));
  }

  const GlanceablesListFooterView* GetListFooterView() const {
    return views::AsViewClass<GlanceablesListFooterView>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kTasksBubbleListFooter)));
  }

  const IconButton* GetHeaderIconView() const {
    return views::AsViewClass<IconButton>(
        view_
            ->GetViewByID(
                base::to_underlying(GlanceablesViewId::kTasksBubbleHeaderView))
            ->GetViewByID(base::to_underlying(
                GlanceablesViewId::kTasksBubbleHeaderIcon)));
  }

  const views::ProgressBar* GetProgressBar() const {
    return views::AsViewClass<views::ProgressBar>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kProgressBar)));
  }

  api::FakeTasksClient* tasks_client() const {
    return fake_glanceables_tasks_client_.get();
  }

  const GlanceablesTestNewWindowDelegate* new_window_delegate() const {
    return &new_window_delegate_;
  }

  void MenuSelectionAt(int index) {
    GetComboBoxView()->SelectMenuItemForTest(index);
  }

 private:
  base::test::ScopedFeatureList feature_list_{features::kGlanceablesV2};
  AccountId account_id_ = AccountId::FromUserEmail("test_user@gmail.com");
  std::unique_ptr<api::FakeTasksClient> fake_glanceables_tasks_client_;
  const GlanceablesTestNewWindowDelegate new_window_delegate_;
  raw_ptr<TasksBubbleView> view_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(TasksBubbleViewTest, ShowTasksComboModel) {
  EXPECT_FALSE(IsMenuRunning());
  EXPECT_TRUE(GetComboBoxView()->GetVisible());

  EXPECT_EQ(GetTaskItemsContainerView()->children().size(), 2u);

  // Verify that tapping on combobox opens the selection menu.
  GestureTapOn(GetComboBoxView());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsMenuRunning());

  EXPECT_EQ(gfx::Size(168, 172), GetComboBoxView()->GetMenuViewSize());
  EXPECT_TRUE(GetComboBoxView()->MenuView()->GetBoundsInScreen().Intersects(
      GetComboBoxView()->MenuItemAtIndex(0)->GetBoundsInScreen()));
  EXPECT_FALSE(GetComboBoxView()->MenuView()->GetBoundsInScreen().Intersects(
      GetComboBoxView()->MenuItemAtIndex(5)->GetBoundsInScreen()));

  // Select the second task list using keyboard navigation.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);

  // Verify the number of items in task_items_container_view()->children().
  EXPECT_EQ(GetTaskItemsContainerView()->children().size(), 3u);

  WaitForTimeBetweenButtonOnClicks();
  // Verify that tapping on combobox opens the selection menu.
  GestureTapOn(GetComboBoxView());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsMenuRunning());

  // Select the first task list using keyboard navigation.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_UP);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);

  // Verify the number of items in task_items_container_view()->children().
  EXPECT_EQ(GetTaskItemsContainerView()->children().size(), 2u);

  WaitForTimeBetweenButtonOnClicks();
  // Verify that tapping on combobox opens the selection menu.
  GestureTapOn(GetComboBoxView());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsMenuRunning());

  // Select the sixth task list using keyboard navigation.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);

  WaitForTimeBetweenButtonOnClicks();
  // Verify that tapping on combobox opens the selection menu.
  GestureTapOn(GetComboBoxView());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsMenuRunning());

  // Verify the number of items in task_items_container_view()->children().
  EXPECT_EQ(GetTaskItemsContainerView()->children().size(), 0u);
  EXPECT_EQ(gfx::Size(168, 172), GetComboBoxView()->GetMenuViewSize());
  EXPECT_FALSE(GetComboBoxView()->MenuView()->GetBoundsInScreen().Intersects(
      GetComboBoxView()->MenuItemAtIndex(0)->GetBoundsInScreen()));
  EXPECT_TRUE(GetComboBoxView()->MenuView()->GetBoundsInScreen().Intersects(
      GetComboBoxView()->MenuItemAtIndex(5)->GetBoundsInScreen()));
}

TEST_F(TasksBubbleViewTest, MarkTaskAsComplete) {
  base::UserActionTester user_actions;
  EXPECT_EQ(GetTaskItemsContainerView()->children().size(), 2u);

  auto* const task_view = views::AsViewClass<GlanceablesTaskView>(
      GetTaskItemsContainerView()->children()[0]);
  ASSERT_TRUE(task_view);
  ASSERT_FALSE(task_view->GetCompletedForTest());
  ASSERT_TRUE(tasks_client()->pending_completed_tasks().empty());

  GestureTapOn(task_view->GetButtonForTest());
  ASSERT_TRUE(task_view->GetCompletedForTest());
  ASSERT_EQ(tasks_client()->pending_completed_tasks().size(), 1u);
  EXPECT_EQ(tasks_client()->pending_completed_tasks().front(),
            "TaskListID1:TaskListItem1");

  EXPECT_EQ(1, user_actions.GetActionCount(
                   "Glanceables_Tasks_TaskMarkedAsCompleted"));
  EXPECT_EQ(0, user_actions.GetActionCount(
                   "Glanceables_Tasks_TaskMarkedAsIncomplete"));
  GestureTapOn(task_view->GetButtonForTest());
  EXPECT_EQ(1, user_actions.GetActionCount(
                   "Glanceables_Tasks_TaskMarkedAsCompleted"));
  EXPECT_EQ(1, user_actions.GetActionCount(
                   "Glanceables_Tasks_TaskMarkedAsIncomplete"));
  GestureTapOn(task_view->GetButtonForTest());
  EXPECT_EQ(2, user_actions.GetActionCount(
                   "Glanceables_Tasks_TaskMarkedAsCompleted"));
  EXPECT_EQ(1, user_actions.GetActionCount(
                   "Glanceables_Tasks_TaskMarkedAsIncomplete"));

  // Tasks should not be marked as completed until closing the bubble.
  EXPECT_EQ(0, tasks_client()->completed_task_count());
  tasks_client()->OnGlanceablesBubbleClosed();
  EXPECT_EQ(1, tasks_client()->completed_task_count());
}

TEST_F(TasksBubbleViewTest, ShowTasksWebUIFromFooterView) {
  base::UserActionTester user_actions;
  const auto* const see_all_button =
      views::AsViewClass<views::LabelButton>(GetListFooterView()->GetViewByID(
          base::to_underlying(GlanceablesViewId::kListFooterSeeAllButton)));
  GestureTapOn(see_all_button);
  EXPECT_EQ(new_window_delegate()->GetLastOpenedUrl(),
            "https://calendar.google.com/calendar/u/0/r/week?opentasks=1");
  EXPECT_EQ(1, user_actions.GetActionCount(
                   "Glanceables_Tasks_LaunchTasksApp_FooterButton"));
  EXPECT_EQ(0, user_actions.GetActionCount(
                   "Glanceables_Tasks_ActiveTaskListChanged"));
}

TEST_F(TasksBubbleViewTest, ShowTasksWebUIFromAddNewButton) {
  base::UserActionTester user_actions;

  ASSERT_EQ(GetComboBoxView()->GetTextForRow(2), u"Task List 3 Title (empty)");
  MenuSelectionAt(2);
  EXPECT_FALSE(GetTaskItemsContainerView()->GetVisible());
  EXPECT_TRUE(GetTaskItemsContainerView()->children().empty());
  EXPECT_TRUE(GetAddNewTaskButton()->GetVisible());

  GestureTapOn(GetAddNewTaskButton());
  EXPECT_EQ(new_window_delegate()->GetLastOpenedUrl(),
            "https://calendar.google.com/calendar/u/0/r/week?opentasks=1");
  EXPECT_EQ(1, user_actions.GetActionCount(
                   "Glanceables_Tasks_LaunchTasksApp_AddNewTaskButton"));
  EXPECT_EQ(
      1, user_actions.GetActionCount("Glanceables_Tasks_AddTaskButtonShown"));
  EXPECT_EQ(1, user_actions.GetActionCount(
                   "Glanceables_Tasks_ActiveTaskListChanged"));
}

TEST_F(TasksBubbleViewTest, ShowTasksWebUIFromHeaderView) {
  base::UserActionTester user_actions;
  const auto* const header_icon_button = GetHeaderIconView();
  GestureTapOn(header_icon_button);
  EXPECT_EQ(new_window_delegate()->GetLastOpenedUrl(),
            "https://calendar.google.com/calendar/u/0/r/week?opentasks=1");
  EXPECT_EQ(1, user_actions.GetActionCount(
                   "Glanceables_Tasks_LaunchTasksApp_HeaderButton"));
  EXPECT_EQ(0, user_actions.GetActionCount(
                   "Glanceables_Tasks_ActiveTaskListChanged"));
}

TEST_F(TasksBubbleViewTest, ShowsAndHidesAddNewButton) {
  base::UserActionTester user_actions;

  // Shows items from the first / default task list.
  EXPECT_TRUE(GetTaskItemsContainerView()->GetVisible());
  EXPECT_EQ(GetTaskItemsContainerView()->children().size(), 2u);
  EXPECT_FALSE(GetAddNewTaskButton()->GetVisible());
  EXPECT_TRUE(GetListFooterView()->GetVisible());

  // Switch to the empty task list.
  ASSERT_EQ(GetComboBoxView()->GetTextForRow(2), u"Task List 3 Title (empty)");
  MenuSelectionAt(2);
  EXPECT_FALSE(GetTaskItemsContainerView()->GetVisible());
  EXPECT_TRUE(GetTaskItemsContainerView()->children().empty());
  EXPECT_TRUE(GetAddNewTaskButton()->GetVisible());
  EXPECT_FALSE(GetListFooterView()->GetVisible());
  EXPECT_EQ(
      1, user_actions.GetActionCount("Glanceables_Tasks_AddTaskButtonShown"));
}

TEST_F(TasksBubbleViewTest, ShowsProgressBarWhileLoadingTasks) {
  ASSERT_TRUE(GetProgressBar());
  ASSERT_TRUE(GetComboBoxView());

  tasks_client()->set_paused(true);

  // Initially progress bar is hidden.
  EXPECT_FALSE(GetProgressBar()->GetVisible());

  // Switch to another task list, the progress bar should become visible.
  MenuSelectionAt(2);
  EXPECT_TRUE(GetProgressBar()->GetVisible());

  // After replying to pending callbacks, the progress bar should become hidden.
  EXPECT_EQ(tasks_client()->RunPendingGetTasksCallbacks(), 1u);
  EXPECT_FALSE(GetProgressBar()->GetVisible());
}

}  // namespace ash
