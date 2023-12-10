// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/api/tasks/fake_tasks_client.h"
#include "ash/constants/ash_features.h"
#include "ash/glanceables/common/glanceables_list_footer_view.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/tasks/glanceables_tasks_view.h"
#include "ash/shell.h"
#include "ash/style/combobox.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

class GlanceablesTasksViewTest : public AshTestBase {
 public:
  GlanceablesTasksViewTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlanceablesV2,
                              features::kGlanceablesTimeManagementStableLaunch},
        /*disabled_features=*/{});
  }

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

    view_ = widget_->SetContentsView(std::make_unique<GlanceablesTasksView>(
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

  const views::View* GetTaskItemsContainerView() const {
    return views::AsViewClass<views::View>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kTasksBubbleListContainer)));
  }

  const views::LabelButton* GetAddNewTaskButton() const {
    return views::AsViewClass<views::LabelButton>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kTasksBubbleAddNewButton)));
  }

  const views::ProgressBar* GetProgressBar() const {
    return views::AsViewClass<views::ProgressBar>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kProgressBar)));
  }

  api::FakeTasksClient* tasks_client() const {
    return fake_glanceables_tasks_client_.get();
  }

  GlanceablesTasksView* view() const { return view_; }

  void MenuSelectionAt(int index) {
    GetComboBoxView()->SelectMenuItemForTest(index);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  AccountId account_id_ = AccountId::FromUserEmail("test_user@gmail.com");
  std::unique_ptr<api::FakeTasksClient> fake_glanceables_tasks_client_;
  raw_ptr<GlanceablesTasksView> view_;
  std::unique_ptr<views::Widget> widget_;
};

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
}

TEST_F(GlanceablesTasksViewTest, ShowsProgressBarWhileEditingTask) {
  tasks_client()->set_paused(true);

  // Initially progress bar is hidden.
  EXPECT_FALSE(GetProgressBar()->GetVisible());

  const auto* const task_items_container_view = GetTaskItemsContainerView();
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
}

TEST_F(GlanceablesTasksViewTest, SupportsEditingRightAfterAdding) {
  tasks_client()->set_paused(true);

  // Add a task.
  GestureTapOn(GetAddNewTaskButton());
  PressAndReleaseKey(ui::VKEY_N, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::VKEY_E);
  PressAndReleaseKey(ui::VKEY_W);
  PressAndReleaseKey(ui::VKEY_ESCAPE);

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

  // Verify executed callbacks number.
  EXPECT_EQ(tasks_client()->RunPendingAddTaskCallbacks(), 0u);
  EXPECT_EQ(tasks_client()->RunPendingUpdateTaskCallbacks(), 1u);
}

TEST_F(GlanceablesTasksViewTest, AllowsAddingOnlyOneTaskAtATime) {
  const auto initial_tasks_count =
      GetTaskItemsContainerView()->children().size();

  // Pressing the "Add new task" button should add another "pending" view.
  GestureTapOn(GetAddNewTaskButton());
  EXPECT_EQ(GetTaskItemsContainerView()->children().size(),
            initial_tasks_count + 1);

  // Pressing the "Add new task" button again does nothing.
  GestureTapOn(GetAddNewTaskButton());
  EXPECT_EQ(GetTaskItemsContainerView()->children().size(),
            initial_tasks_count + 1);
}

TEST_F(GlanceablesTasksViewTest, AddingNewTaskControlsButtonDisabledState) {
  // Initially the button is enabled.
  EXPECT_EQ(GetAddNewTaskButton()->GetState(),
            views::Button::ButtonState::STATE_NORMAL);

  // Pressing the "Add new task" button should disable the button.
  GestureTapOn(GetAddNewTaskButton());
  EXPECT_EQ(GetAddNewTaskButton()->GetState(),
            views::Button::ButtonState::STATE_DISABLED);

  // Exiting edit state should re-enable the button.
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_EQ(GetAddNewTaskButton()->GetState(),
            views::Button::ButtonState::STATE_NORMAL);
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

  const auto* const task_view = GetTaskItemsContainerView()->children()[0];

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

  // Verify executed callbacks number.
  EXPECT_EQ(GetTaskItemsContainerView()->children().size(),
            initial_tasks_count);
  EXPECT_EQ(tasks_client()->RunPendingAddTaskCallbacks(), 0u);
}

}  // namespace ash
