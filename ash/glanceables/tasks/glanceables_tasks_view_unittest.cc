// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/tasks/glanceables_tasks_view.h"

#include <memory>

#include "ash/api/tasks/fake_tasks_client.h"
#include "ash/glanceables/common/glanceables_list_footer_view.h"
#include "ash/glanceables/common/glanceables_util.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/common/test/glanceables_test_new_window_delegate.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/tasks/glanceables_task_view.h"
#include "ash/glanceables/tasks/test/glanceables_tasks_test_util.h"
#include "ash/shell.h"
#include "ash/style/combobox.h"
#include "ash/style/counter_expand_button.h"
#include "ash/style/icon_button.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/gtest_tags.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

class GlanceablesTasksViewTest : public AshTestBase {
 public:
  GlanceablesTasksViewTest()
      : AshTestBase(std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME)) {}

  void SetUp() override {
    AshTestBase::SetUp();
    SimulateUserLogin(account_id_);
    fake_glanceables_tasks_client_ =
        glanceables_tasks_test_util::InitializeFakeTasksClient(
            base::Time::Now());
    fake_glanceables_tasks_client_->set_http_error(
        google_apis::ApiErrorCode::HTTP_SUCCESS);
    Shell::Get()->glanceables_controller()->UpdateClientsRegistration(
        account_id_, GlanceablesController::ClientsRegistration{
                         .tasks_client = fake_glanceables_tasks_client_.get()});
    ASSERT_TRUE(Shell::Get()->glanceables_controller()->GetTasksClient());

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);

    view_ = widget_->SetContentsView(std::make_unique<GlanceablesTasksView>(
        fake_glanceables_tasks_client_->task_lists()));

    glanceables_util::SetIsNetworkConnectedForTest(true);
  }

  void TearDown() override {
    // Destroy `widget_` first, before destroying `LayoutProvider` (needed in
    // the `views::Combobox`'s destruction chain).
    CloseWidget();
    AshTestBase::TearDown();
  }

  // Populates `num` of tasks to the default task list.
  void PopulateTasks(size_t num, std::string task_list_id = "TaskListID1") {
    for (size_t i = 0; i < num; ++i) {
      auto num_string = base::NumberToString(i);
      fake_glanceables_tasks_client_->AddTask(
          task_list_id, base::StrCat({"title_", num_string}),
          base::DoNothing());
    }

    // Simulate closing the glanceables bubble to cache the tasks.
    fake_glanceables_tasks_client_->OnGlanceablesBubbleClosed(
        base::DoNothing());

    // Recreate the tasks view to update the task views.
    view_ = widget_->SetContentsView(std::make_unique<GlanceablesTasksView>(
        fake_glanceables_tasks_client_->task_lists()));
  }

  void CloseWidget() {
    view_ = nullptr;
    widget_.reset();
  }

  Combobox* GetComboBoxView() const {
    return views::AsViewClass<Combobox>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kTimeManagementBubbleComboBox)));
  }

  const IconButton* GetHeaderIconView() const {
    return views::AsViewClass<IconButton>(
        view_->GetViewByID(base::to_underlying(
            GlanceablesViewId::kTimeManagementBubbleHeaderIcon)));
  }

  const CounterExpandButton* GetCounterExpandButton() const {
    return views::AsViewClass<CounterExpandButton>(
        view_->GetViewByID(base::to_underlying(
            GlanceablesViewId::kTimeManagementBubbleExpandButton)));
  }

  views::ScrollView* GetScrollView() const {
    return views::AsViewClass<views::ScrollView>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kContentsScrollView)));
  }

  const views::View* GetTaskItemsContainerView() const {
    return views::AsViewClass<views::View>(
        view_->GetViewByID(base::to_underlying(
            GlanceablesViewId::kTimeManagementBubbleListContainer)));
  }

  const views::View* GetEditInBrowserButton() const {
    return views::AsViewClass<views::View>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kTaskItemEditInBrowserLabel)));
  }

  const views::LabelButton* GetAddNewTaskButton() const {
    return views::AsViewClass<views::LabelButton>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kTasksBubbleAddNewButton)));
  }

  const GlanceablesListFooterView* GetListFooterView() const {
    return views::AsViewClass<GlanceablesListFooterView>(
        view_->GetViewByID(base::to_underlying(
            GlanceablesViewId::kTimeManagementBubbleListFooter)));
  }

  const views::ProgressBar* GetProgressBar() const {
    return views::AsViewClass<views::ProgressBar>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kProgressBar)));
  }

  const ErrorMessageToast* GetErrorMessage() const {
    return views::AsViewClass<ErrorMessageToast>(
        view_->GetViewByID(base::to_underlying(
            GlanceablesViewId::kTimeManagementErrorMessageToast)));
  }

  api::FakeTasksClient* tasks_client() const {
    return fake_glanceables_tasks_client_.get();
  }

  const GlanceablesTestNewWindowDelegate* new_window_delegate() const {
    return &new_window_delegate_;
  }

  GlanceablesTasksView* view() const { return view_; }

  void MenuSelectionAt(int index) {
    GetComboBoxView()->SelectMenuItemForTest(index);
  }

 private:
  AccountId account_id_ = AccountId::FromUserEmail("test_user@gmail.com");
  std::unique_ptr<api::FakeTasksClient> fake_glanceables_tasks_client_;
  raw_ptr<GlanceablesTasksView, DanglingUntriaged> view_;
  std::unique_ptr<views::Widget> widget_;

  const GlanceablesTestNewWindowDelegate new_window_delegate_;
};

TEST_F(GlanceablesTasksViewTest, Basics) {
  // Check that `GlanceablesTasksView` by itself doesn't have a background.
  EXPECT_FALSE(view()->GetBackground());

  // Check that the expand button does not exist when `GlanceablesTasksView` is
  // created alone.
  auto* expand_button = view()->GetViewByID(base::to_underlying(
      GlanceablesViewId::kTimeManagementBubbleExpandButton));
  EXPECT_TRUE(expand_button);
  EXPECT_FALSE(expand_button->GetVisible());
}

TEST_F(GlanceablesTasksViewTest, RecordShowTimeHistogramOnClose) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "Ash.Glanceables.TimeManagement.Tasks.TotalShowTime", 0);

  CloseWidget();

  histogram_tester.ExpectTotalCount(
      "Ash.Glanceables.TimeManagement.Tasks.TotalShowTime", 1);
}

TEST_F(GlanceablesTasksViewTest, ShowsProgressBarWhileLoadingTasks) {
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

TEST_F(GlanceablesTasksViewTest, ShowsProgressBarWhileAddingTask) {
  base::HistogramTester histogram_tester;
  tasks_client()->set_paused(true);

  // Initially progress bar is hidden.
  EXPECT_FALSE(GetProgressBar()->GetVisible());

  GestureTapOn(GetAddNewTaskButton());
  PressAndReleaseKey(ui::VKEY_N, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::VKEY_E);
  PressAndReleaseKey(ui::VKEY_W);

  // Progress bar becomes visible during saving.
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_TRUE(GetProgressBar()->GetVisible());

  // After replying to pending callbacks, the progress bar should become hidden.
  EXPECT_EQ(tasks_client()->RunPendingAddTaskCallbacks(), 1u);
  EXPECT_FALSE(GetProgressBar()->GetVisible());

  histogram_tester.ExpectUniqueSample(
      "Ash.Glanceables.TimeManagement.Tasks.UserAction", 3, 1);
}

TEST_F(GlanceablesTasksViewTest, ShowsProgressBarWhileEditingTask) {
  base::HistogramTester histogram_tester;
  tasks_client()->set_paused(true);

  // Initially progress bar is hidden.
  EXPECT_FALSE(GetProgressBar()->GetVisible());

  const auto* const task_items_container_view = GetTaskItemsContainerView();
  EXPECT_EQ(GetCounterExpandButton()->counter_for_test(), 2u);
  EXPECT_EQ(task_items_container_view->children().size(), 2u);

  const auto* const title_label = views::AsViewClass<views::Label>(
      task_items_container_view->children()[0]->GetViewByID(
          base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));
  GestureTapOn(title_label);
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_SPACE);
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_U);
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_P);
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_D);

  // Progress bar becomes visible during saving.
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_TRUE(GetProgressBar()->GetVisible());

  // After replying to pending callbacks, the progress bar should become hidden.
  EXPECT_EQ(tasks_client()->RunPendingUpdateTaskCallbacks(), 1u);
  EXPECT_FALSE(GetProgressBar()->GetVisible());

  histogram_tester.ExpectUniqueSample(
      "Ash.Glanceables.TimeManagement.Tasks.UserAction", 4, 1);
}

TEST_F(GlanceablesTasksViewTest, ScrollViewResetPositionAfterSwitchingLists) {
  PopulateTasks(20, "TaskListID1");
  PopulateTasks(20, "TaskListID2");

  auto* scroll_bar = GetScrollView()->vertical_scroll_bar();
  EXPECT_EQ(scroll_bar->GetPosition(), scroll_bar->GetMinPosition());
  ASSERT_TRUE(scroll_bar->GetVisible());
  scroll_bar->ScrollByAmount(views::ScrollBar::ScrollAmount::kEnd);
  EXPECT_GT(scroll_bar->GetPosition(), scroll_bar->GetMinPosition());

  GetComboBoxView()->SelectMenuItemForTest(1);
  EXPECT_EQ(scroll_bar->GetPosition(), scroll_bar->GetMinPosition());
}

TEST_F(GlanceablesTasksViewTest, OnlyShowsFooterIfAtLeast100Tasks) {
  ASSERT_TRUE(GetListFooterView());
  EXPECT_FALSE(GetListFooterView()->GetVisible());

  const auto initial_tasks_count =
      GetTaskItemsContainerView()->children().size();
  // Add tasks to make the list contain 99 tasks.
  PopulateTasks(99u - initial_tasks_count);
  view()->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_FALSE(GetListFooterView()->GetVisible());

  // Creates the 100th task.
  GestureTapOn(GetAddNewTaskButton());
  PressAndReleaseKey(ui::VKEY_N, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::VKEY_E);
  PressAndReleaseKey(ui::VKEY_W);
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  view()->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_TRUE(GetListFooterView()->GetVisible());
}

TEST_F(GlanceablesTasksViewTest, SupportsEditingRightAfterAdding) {
  base::HistogramTester histogram_tester;
  tasks_client()->set_paused(true);

  // Add a task.
  GestureTapOn(GetAddNewTaskButton());
  PressAndReleaseKey(ui::VKEY_N, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::VKEY_E);
  PressAndReleaseKey(ui::VKEY_W);
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  base::RunLoop().RunUntilIdle();

  // Verify executed callbacks number.
  EXPECT_EQ(tasks_client()->RunPendingAddTaskCallbacks(), 1u);
  EXPECT_EQ(tasks_client()->RunPendingUpdateTaskCallbacks(), 0u);

  view()->GetWidget()->LayoutRootViewIfNecessary();

  // Edit the same task.
  const auto* const title_label = views::AsViewClass<views::Label>(
      GetTaskItemsContainerView()->children()[0]->GetViewByID(
          base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));

  GestureTapOn(title_label);
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_SPACE);
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_1);
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  base::RunLoop().RunUntilIdle();

  // Verify executed callbacks number.
  EXPECT_EQ(tasks_client()->RunPendingAddTaskCallbacks(), 0u);
  EXPECT_EQ(tasks_client()->RunPendingUpdateTaskCallbacks(), 1u);

  histogram_tester.ExpectTotalCount(
      "Ash.Glanceables.TimeManagement.Tasks.UserAction", 2);
  histogram_tester.ExpectBucketCount(
      "Ash.Glanceables.TimeManagement.Tasks.UserAction", 3, 1);
  histogram_tester.ExpectBucketCount(
      "Ash.Glanceables.TimeManagement.Tasks.UserAction", 4, 1);
}

TEST_F(GlanceablesTasksViewTest, TabbingOutOfNewTaskTextfieldAddsTask) {
  base::HistogramTester histogram_tester;
  tasks_client()->set_paused(true);

  // Add a task.
  GestureTapOn(GetAddNewTaskButton());

  const auto* task_view = GetTaskItemsContainerView()->children()[0].get();
  EXPECT_TRUE(task_view->GetViewByID(
      base::to_underlying(GlanceablesViewId::kTaskItemTitleTextField)));

  PressAndReleaseKey(ui::VKEY_N, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::VKEY_E);
  PressAndReleaseKey(ui::VKEY_W);
  PressAndReleaseKey(ui::VKEY_TAB);
  base::RunLoop().RunUntilIdle();

  // Verify that edit in browser button is visible and focused.
  const auto* const edit_in_browser_button = GetEditInBrowserButton();
  ASSERT_TRUE(edit_in_browser_button);
  EXPECT_TRUE(edit_in_browser_button->GetVisible());
  EXPECT_TRUE(edit_in_browser_button->HasFocus());

  const auto* title_label =
      views::AsViewClass<views::Label>(task_view->GetViewByID(
          base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));
  EXPECT_FALSE(title_label);

  // Verify executed callbacks number.
  EXPECT_EQ(tasks_client()->RunPendingAddTaskCallbacks(), 1u);
  EXPECT_EQ(tasks_client()->RunPendingUpdateTaskCallbacks(), 0u);

  // Tab back to the Add task textfield, and update the text.
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  base::RunLoop().RunUntilIdle();

  const auto* title_text_field =
      views::AsViewClass<views::Textfield>(task_view->GetViewByID(
          base::to_underlying(GlanceablesViewId::kTaskItemTitleTextField)));
  ASSERT_TRUE(title_text_field);
  EXPECT_TRUE(title_text_field->HasFocus());
  EXPECT_EQ(u"New", title_text_field->GetText());

  PressAndReleaseKey(ui::VKEY_RIGHT);
  PressAndReleaseKey(ui::VKEY_1);
  // Focus edit in browser button.
  PressAndReleaseKey(ui::VKEY_TAB);

  EXPECT_EQ(tasks_client()->RunPendingAddTaskCallbacks(), 0u);
  EXPECT_EQ(tasks_client()->RunPendingUpdateTaskCallbacks(), 1u);

  // Focus the next task, which exits the task editing state.
  PressAndReleaseKey(ui::VKEY_TAB);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(GetEditInBrowserButton());

  task_view = GetTaskItemsContainerView()->children()[0].get();
  title_text_field =
      views::AsViewClass<views::Textfield>(task_view->GetViewByID(
          base::to_underlying(GlanceablesViewId::kTaskItemTitleTextField)));
  EXPECT_FALSE(title_text_field);

  title_label = views::AsViewClass<views::Label>(task_view->GetViewByID(
      base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));
  ASSERT_TRUE(title_label);
  EXPECT_TRUE(title_label->IsDrawn());
  EXPECT_EQ(u"New1", title_label->GetText());

  // Edit the same task.
  view()->GetWidget()->LayoutRootViewIfNecessary();
  GestureTapOn(title_label);
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_SPACE);
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_1);
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  base::RunLoop().RunUntilIdle();

  // Verify executed callbacks number.
  EXPECT_EQ(tasks_client()->RunPendingAddTaskCallbacks(), 0u);
  EXPECT_EQ(tasks_client()->RunPendingUpdateTaskCallbacks(), 1u);

  title_label = views::AsViewClass<views::Label>(
      GetTaskItemsContainerView()->children()[0]->GetViewByID(
          base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));
  ASSERT_TRUE(title_label);
  EXPECT_TRUE(title_label->IsDrawn());
  EXPECT_EQ(u"New1 1", title_label->GetText());

  histogram_tester.ExpectTotalCount(
      "Ash.Glanceables.TimeManagement.Tasks.UserAction", 2);
  histogram_tester.ExpectBucketCount(
      "Ash.Glanceables.TimeManagement.Tasks.UserAction", 3, 1);
  histogram_tester.ExpectBucketCount(
      "Ash.Glanceables.TimeManagement.Tasks.UserAction", 4, 1);
}

TEST_F(GlanceablesTasksViewTest, AllowsPressingAddNewTaskButtonWhileAdding) {
  const auto initial_tasks_count =
      GetTaskItemsContainerView()->children().size();

  // Pressing the "Add new task" button should add another "pending" view.
  GestureTapOn(GetAddNewTaskButton());
  EXPECT_EQ(GetTaskItemsContainerView()->children().size(),
            initial_tasks_count + 1);

  // Enter text without explicitly committing it.
  PressAndReleaseKey(ui::VKEY_N, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::VKEY_E);
  PressAndReleaseKey(ui::VKEY_W);

  // Pressing the "Add new task" button again adds another "pending" view.
  GestureTapOn(GetAddNewTaskButton());
  EXPECT_EQ(GetTaskItemsContainerView()->children().size(),
            initial_tasks_count + 2);

  base::RunLoop().RunUntilIdle();

  // But the previous task becomes automatically committed due to losing focus.
  const auto* const previous_task_label = views::AsViewClass<views::Label>(
      GetTaskItemsContainerView()->children()[1]->GetViewByID(
          base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));
  ASSERT_TRUE(previous_task_label);
  EXPECT_EQ(previous_task_label->GetText(), u"New");
}

TEST_F(GlanceablesTasksViewTest,
       DoesNotSendRequestAfterEditingWithUnchangedTitle) {
  tasks_client()->set_paused(true);

  const auto* const title_label = views::AsViewClass<views::Label>(
      GetTaskItemsContainerView()->children()[0]->GetViewByID(
          base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));

  // Enter and exit editing mode, the task's title should stay the same.
  GestureTapOn(title_label);
  PressAndReleaseKey(ui::VKEY_ESCAPE);

  // Verify executed callbacks number.
  EXPECT_EQ(tasks_client()->RunPendingUpdateTaskCallbacks(), 0u);
}

TEST_F(GlanceablesTasksViewTest, DoesNotAllowEditingToBlankTitle) {
  tasks_client()->set_paused(true);

  const auto* const task_view =
      GetTaskItemsContainerView()->children()[0].get();

  {
    const auto* const title_label =
        views::AsViewClass<views::Label>(task_view->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));
    EXPECT_FALSE(title_label->GetText().empty());

    // Enter editing mode.
    GestureTapOn(title_label);
  }

  {
    const auto* const title_text_field =
        views::AsViewClass<views::Textfield>(task_view->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemTitleTextField)));
    EXPECT_FALSE(title_text_field->GetText().empty());

    // Clear `title_text_field`.
    PressAndReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
    PressAndReleaseKey(ui::VKEY_DELETE);
    EXPECT_TRUE(title_text_field->GetText().empty());

    // Commit changes.
    PressAndReleaseKey(ui::VKEY_ESCAPE);

    base::RunLoop().RunUntilIdle();
  }

  {
    const auto* const title_label =
        views::AsViewClass<views::Label>(task_view->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));

    // `title_label` is back with non-empty title.
    EXPECT_FALSE(title_label->GetText().empty());
  }

  // Verify executed callbacks number.
  EXPECT_EQ(tasks_client()->RunPendingUpdateTaskCallbacks(), 0u);
}

TEST_F(GlanceablesTasksViewTest, DoesNotAddTaskWithBlankTitle) {
  tasks_client()->set_paused(true);

  const auto initial_tasks_count =
      GetTaskItemsContainerView()->children().size();

  // Add a task with blank title.
  GestureTapOn(GetAddNewTaskButton());
  EXPECT_EQ(GetTaskItemsContainerView()->children().size(),
            initial_tasks_count + 1);
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  base::RunLoop().RunUntilIdle();

  // Verify executed callbacks number.
  EXPECT_EQ(GetCounterExpandButton()->counter_for_test(), initial_tasks_count);
  EXPECT_EQ(GetTaskItemsContainerView()->children().size(),
            initial_tasks_count);
  EXPECT_EQ(tasks_client()->RunPendingAddTaskCallbacks(), 0u);
}

TEST_F(GlanceablesTasksViewTest, ComboboxExpandedCollapsedAccessibleState) {
  auto* combobox = GetComboBoxView();

  ui::AXNodeData node_data;
  combobox->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kCollapsed));

  // Check accessibility of combobox while it's open.
  LeftClickOn(combobox);
  node_data = ui::AXNodeData();
  combobox->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kCollapsed));

  // Check accessibility of combobox while it's closed.
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_ESCAPE);
  node_data = ui::AXNodeData();
  combobox->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kCollapsed));
}

TEST_F(GlanceablesTasksViewTest, OpenBrowserWithEmptyNewTaskDoesntCrash) {
  base::UserActionTester user_actions;

  // Add a task with blank title.
  GestureTapOn(GetAddNewTaskButton());

  GestureTapOn(GetHeaderIconView());
  EXPECT_EQ(1, user_actions.GetActionCount(
                   "Glanceables_Tasks_LaunchTasksApp_HeaderButton"));

  // Simulate that the widget is hidden safely after opening a browser window.
  view()->GetWidget()->Hide();
  EXPECT_FALSE(view()->GetWidget()->GetNativeWindow()->IsVisible());
}

TEST_F(GlanceablesTasksViewTest, HandlesErrorAfterAdding) {
  tasks_client()->set_paused(true);
  tasks_client()->set_http_error(
      google_apis::ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR);

  const auto* const task_items_container_view = GetTaskItemsContainerView();
  ASSERT_TRUE(task_items_container_view);

  EXPECT_EQ(task_items_container_view->children().size(), 2u);
  EXPECT_FALSE(GetErrorMessage());

  GestureTapOn(GetAddNewTaskButton());
  PressAndReleaseKey(ui::VKEY_N, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::VKEY_E);
  PressAndReleaseKey(ui::VKEY_W);
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(task_items_container_view->children().size(), 3u);
  EXPECT_FALSE(GetErrorMessage());

  EXPECT_EQ(tasks_client()->RunPendingAddTaskCallbacks(), 1u);
  EXPECT_EQ(task_items_container_view->children().size(), 2u);
  EXPECT_TRUE(GetErrorMessage());
  EXPECT_EQ(GetErrorMessage()->GetMessageForTest(), u"Couldn't edit task.");
  EXPECT_EQ(GetErrorMessage()->GetButtonForTest()->GetText(), u"Dismiss");
}

TEST_F(GlanceablesTasksViewTest, HandlesErrorAfterEditing) {
  tasks_client()->set_paused(true);
  tasks_client()->set_http_error(
      google_apis::ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR);

  const auto* const task_items_container_view = GetTaskItemsContainerView();
  ASSERT_TRUE(task_items_container_view);

  EXPECT_EQ(task_items_container_view->children().size(), 2u);
  EXPECT_FALSE(GetErrorMessage());

  const auto* title_label = views::AsViewClass<views::Label>(
      task_items_container_view->children()[0]->GetViewByID(
          base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));

  GestureTapOn(title_label);
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_SPACE);
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_U);
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_P);
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_D);
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(task_items_container_view->children().size(), 2u);
  EXPECT_FALSE(GetErrorMessage());

  EXPECT_EQ(tasks_client()->RunPendingUpdateTaskCallbacks(), 1u);
  EXPECT_EQ(task_items_container_view->children().size(), 2u);
  EXPECT_TRUE(GetErrorMessage());
  EXPECT_EQ(GetErrorMessage()->GetMessageForTest(), u"Couldn't edit task.");
  EXPECT_EQ(GetErrorMessage()->GetButtonForTest()->GetText(), u"Dismiss");

  // Revert the task title to the one before editing.
  title_label = views::AsViewClass<views::Label>(
      task_items_container_view->children()[0]->GetViewByID(
          base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));
  EXPECT_EQ(title_label->GetText(), u"Task List 1 Item 1 Title");
}

TEST_F(GlanceablesTasksViewTest, HandlesErrorAfterChangingTaskList) {
  const auto* const task_items_container_view = GetTaskItemsContainerView();
  ASSERT_TRUE(task_items_container_view);
  EXPECT_FALSE(GetErrorMessage());

  // Disconnect the network for test.
  glanceables_util::SetIsNetworkConnectedForTest(false);

  // Switch to another task list. The error message should show up immediately
  // and ask users to try again after connecting to the network.
  MenuSelectionAt(2);
  EXPECT_TRUE(GetErrorMessage());
  EXPECT_EQ(GetErrorMessage()->GetMessageForTest(),
            u"Couldn't load items. Try again when online.");
  EXPECT_EQ(GetErrorMessage()->GetButtonForTest()->GetText(), u"Dismiss");

  // The task list should be reset to the one before switch.
  const std::optional<size_t> selected_index =
      GetComboBoxView()->GetSelectedIndex();
  ASSERT_TRUE(selected_index.has_value());
  EXPECT_EQ(GetComboBoxView()->GetTextForRow(selected_index.value()),
            u"Task List 1 Title");
}

TEST_F(GlanceablesTasksViewTest, TasksContainerIsInvisibleWhenNoTask) {
  // Check that task list items from the first list are shown.
  auto* combobox = GetComboBoxView();
  EXPECT_EQ(combobox->GetTextForRow(combobox->GetSelectedIndex().value()),
            u"Task List 1 Title");

  // Click on the combo box to show the task lists.
  LeftClickOn(combobox);

  // Go to the list with no task in it.
  auto* third_menu_item_label = combobox->MenuItemAtIndex(2);
  ASSERT_TRUE(third_menu_item_label);
  LeftClickOn(third_menu_item_label);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(combobox->GetTextForRow(combobox->GetSelectedIndex().value()),
            u"Task List 3 Title (empty)");

  const auto* const task_items_container = GetTaskItemsContainerView();
  EXPECT_EQ(GetCounterExpandButton()->counter_for_test(), 0u);
  EXPECT_EQ(task_items_container->children().size(), 0u);
  EXPECT_FALSE(task_items_container->GetVisible());

  // Click on the "Add a task" button. The task container should be visible now.
  auto* add_task_button = GetAddNewTaskButton();
  ASSERT_TRUE(add_task_button);
  LeftClickOn(add_task_button);
  EXPECT_EQ(task_items_container->children().size(), 1u);
  EXPECT_TRUE(task_items_container->GetVisible());

  // Commit the empty new task, which removes the temporary task view. The task
  // container is reset to invisible.
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_ESCAPE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(task_items_container->children().size(), 0u);
  EXPECT_FALSE(task_items_container->GetVisible());
}

TEST_F(GlanceablesTasksViewTest, ShowTasksWebUIFromHeaderView) {
  base::UserActionTester user_actions;
  const auto* const header_icon_button = GetHeaderIconView();
  GestureTapOn(header_icon_button);
  EXPECT_EQ(new_window_delegate()->GetLastOpenedUrl(),
            "https://tasks.google.com/");
  EXPECT_EQ(1, user_actions.GetActionCount(
                   "Glanceables_Tasks_LaunchTasksApp_HeaderButton"));
  EXPECT_EQ(0, user_actions.GetActionCount(
                   "Glanceables_Tasks_ActiveTaskListChanged"));
}

TEST_F(GlanceablesTasksViewTest, ShowTasksWebUIFromEditInBrowserView) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-75d32091-1825-49cb-843b-8bb9d998a47d");

  base::HistogramTester histogram_tester;
  base::UserActionTester user_actions;
  const auto* const title_label = views::AsViewClass<views::Label>(
      GetTaskItemsContainerView()->children()[0]->GetViewByID(
          base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));

  // Tap the title label to enter the edit mode. The enter in browser button
  // should be visible.
  GestureTapOn(title_label);
  view()->GetWidget()->LayoutRootViewIfNecessary();
  const auto* const edit_in_browser_button = GetEditInBrowserButton();
  ASSERT_TRUE(edit_in_browser_button);
  EXPECT_TRUE(edit_in_browser_button->GetVisible());

  // Verify that tapping on the button will record the action.
  GestureTapOn(edit_in_browser_button);
  EXPECT_EQ(new_window_delegate()->GetLastOpenedUrl(),
            "https://tasks.google.com/task/TaskListItem1");
  EXPECT_EQ(1, user_actions.GetActionCount(
                   "Glanceables_Tasks_LaunchTasksApp_EditInGoogleTasksButton"));
  histogram_tester.ExpectTotalCount(
      "Ash.Glanceables.TimeManagement.Tasks.UserAction", 2);
  histogram_tester.ExpectBucketCount(
      "Ash.Glanceables.TimeManagement.Tasks.UserAction", 4, 1);
  histogram_tester.ExpectBucketCount(
      "Ash.Glanceables.TimeManagement.Tasks.UserAction", 8, 1);

  // Simulate that the widget is hidden safely after opening a browser window.
  view()->GetWidget()->Hide();
  EXPECT_FALSE(view()->GetWidget()->GetNativeWindow()->IsVisible());
}

TEST_F(GlanceablesTasksViewTest, ComboboxAccessibleActiveDescendantId) {
  auto* combobox = GetComboBoxView();
  ui::AXNodeData node_data;
  base::test::TaskEnvironment* task_environment_ = task_environment();

  // Combobox is closed initially.
  ASSERT_FALSE(
      node_data.HasIntAttribute(ax::mojom::IntAttribute::kActivedescendantId));

  // Check accessibility of combobox when it is open.
  LeftClickOn(combobox);
  combobox->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  ASSERT_TRUE(combobox->MenuItemAtIndex(0));
  ASSERT_TRUE(
      node_data.HasIntAttribute(ax::mojom::IntAttribute::kActivedescendantId));
  ASSERT_EQ(
      node_data.GetIntAttribute(ax::mojom::IntAttribute::kActivedescendantId),
      combobox->MenuItemAtIndex(0)->GetViewAccessibility().GetUniqueId());

  // Select second item in combobox menu items.
  MenuSelectionAt(1);
  // Advance time so that subsequent mouse click is considered valid.
  task_environment_->AdvanceClock(views::kMinimumTimeBetweenButtonClicks +
                                  base::Milliseconds(10));

  LeftClickOn(combobox);  // Open combobox.
  node_data = ui::AXNodeData();
  combobox->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  ASSERT_TRUE(combobox->MenuItemAtIndex(1));
  ASSERT_TRUE(
      node_data.HasIntAttribute(ax::mojom::IntAttribute::kActivedescendantId));
  ASSERT_EQ(
      node_data.GetIntAttribute(ax::mojom::IntAttribute::kActivedescendantId),
      combobox->MenuItemAtIndex(1)->GetViewAccessibility().GetUniqueId());

  // Check accessibility of combobox when it is closed.
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_ESCAPE);
  node_data = ui::AXNodeData();
  combobox->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  ASSERT_FALSE(
      node_data.HasIntAttribute(ax::mojom::IntAttribute::kActivedescendantId));
}

TEST_F(GlanceablesTasksViewTest, ComboboxAccessibleValue) {
  auto* combobox = GetComboBoxView();

  // default selection is first item in combobox
  ui::AXNodeData node_data;
  combobox->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ("Task List 1 Title",
            node_data.GetStringAttribute(ax::mojom::StringAttribute::kValue));

  // Select second item in combobox menu items.
  MenuSelectionAt(1);
  node_data = ui::AXNodeData();
  combobox->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ("Task List 2 Title",
            node_data.GetStringAttribute(ax::mojom::StringAttribute::kValue));

  // Select third item in combobox menu items.
  MenuSelectionAt(2);
  node_data = ui::AXNodeData();
  combobox->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ("Task List 3 Title (empty)",
            node_data.GetStringAttribute(ax::mojom::StringAttribute::kValue));
}

}  // namespace ash
