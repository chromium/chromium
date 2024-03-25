// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/tasks/glanceables_task_view.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/api/tasks/tasks_client.h"
#include "ash/api/tasks/tasks_types.h"
#include "ash/constants/ash_features.h"
#include "ash/glanceables/common/glanceables_util.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/types/cxx23_to_underlying.h"
#include "chromeos/ash/components/settings/scoped_timezone_settings.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {

class GlanceablesTaskViewTest : public AshTestBase {
 public:
  GlanceablesTaskViewTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlanceablesTimeManagementTasksView},
        /*disabled_features=*/{});
    glanceables_util::SetIsNetworkConnectedForTest(true);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(GlanceablesTaskViewTest, FormatsDueDate) {
  base::subtle::ScopedTimeClockOverrides time_override(
      []() {
        base::Time now;
        EXPECT_TRUE(base::Time::FromString("2022-12-21T13:25:00.000Z", &now));
        return now;
      },
      nullptr, nullptr);

  struct {
    std::string due;
    std::string time_zone;
    std::u16string expected_text;
  } test_cases[] = {
      {"2022-12-21T00:00:00.000Z", "America/New_York", u"Today"},
      {"2022-12-21T00:00:00.000Z", "Europe/Oslo", u"Today"},
      {"2022-12-30T00:00:00.000Z", "America/New_York", u"Fri, Dec 30"},
      {"2022-12-30T00:00:00.000Z", "Europe/Oslo", u"Fri, Dec 30"},
  };

  for (const auto& tc : test_cases) {
    // 1 - for ICU formatters; 2 - for `base::Time::LocalExplode`.
    system::ScopedTimezoneSettings tz(base::UTF8ToUTF16(tc.time_zone));
    calendar_test_utils::ScopedLibcTimeZone libc_tz(tc.time_zone);

    base::Time due;
    EXPECT_TRUE(base::Time::FromString(tc.due.c_str(), &due));

    const auto task = api::Task("task-id", "Task title",
                                /*due=*/due, /*completed=*/false,
                                /*has_subtasks=*/false,
                                /*has_email_link=*/false, /*has_notes=*/false,
                                /*updated=*/due, /*web_view_link=*/GURL());
    const auto view = GlanceablesTaskView(
        &task, /*mark_as_completed_callback=*/base::DoNothing(),
        /*save_callback=*/base::DoNothing(),
        /*edit_in_browser_callback=*/base::DoNothing(),
        /*show_error_message_callback=*/base::DoNothing());

    const auto* const due_label =
        views::AsViewClass<views::Label>(view.GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemDueLabel)));
    ASSERT_TRUE(due_label);

    EXPECT_EQ(due_label->GetText(), tc.expected_text);
  }
}

TEST_F(GlanceablesTaskViewTest,
       AppliesStrikeThroughStyleAfterMarkingAsComplete) {
  const auto task = api::Task("task-id", "Task title",
                              /*due=*/std::nullopt, /*completed=*/false,
                              /*has_subtasks=*/false, /*has_email_link=*/false,
                              /*has_notes=*/false, /*updated=*/base::Time(),
                              /*web_view_link=*/GURL());

  const auto widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  const auto* const view =
      widget->SetContentsView(std::make_unique<GlanceablesTaskView>(
          &task, /*mark_as_completed_callback=*/base::DoNothing(),
          /*save_callback=*/base::DoNothing(),
          /*edit_in_browser_callback=*/base::DoNothing(),
          /*show_error_message_callback=*/base::DoNothing()));
  ASSERT_TRUE(view);

  const auto* const checkbox = view->GetCheckButtonForTest();
  ASSERT_TRUE(checkbox);

  const auto* const title_label =
      views::AsViewClass<views::Label>(view->GetViewByID(
          base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));
  ASSERT_TRUE(title_label);

  // No `STRIKE_THROUGH` style applied initially.
  EXPECT_FALSE(view->GetCompletedForTest());
  EXPECT_FALSE(title_label->font_list().GetFontStyle() &
               gfx::Font::FontStyle::STRIKE_THROUGH);

  // After pressing on `checkbox`, the label should have `STRIKE_THROUGH` style
  // applied.
  GestureTapOn(checkbox);
  EXPECT_TRUE(view->GetCompletedForTest());
  EXPECT_TRUE(title_label->font_list().GetFontStyle() &
              gfx::Font::FontStyle::STRIKE_THROUGH);
}

TEST_F(GlanceablesTaskViewTest, UpdatingTaskTriggersErrorMessageIfNoNetwork) {
  // Simulate that the network is disabled.
  glanceables_util::SetIsNetworkConnectedForTest(false);

  const auto task = api::Task("task-id", "Task title",
                              /*due=*/std::nullopt, /*completed=*/false,
                              /*has_subtasks=*/false, /*has_email_link=*/false,
                              /*has_notes=*/false, /*updated=*/base::Time(),
                              /*web_view_link=*/GURL());

  const auto widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  base::test::TestFuture<GlanceablesTasksErrorType,
                         GlanceablesErrorMessageView::ButtonActionType>
      error_future;

  const auto* const view =
      widget->SetContentsView(std::make_unique<GlanceablesTaskView>(
          &task, /*mark_as_completed_callback=*/base::DoNothing(),
          /*save_callback=*/base::DoNothing(),
          /*edit_in_browser_callback=*/base::DoNothing(),
          /*show_error_message_callback=*/error_future.GetRepeatingCallback()));
  ASSERT_TRUE(view);

  const auto* const checkbox = view->GetCheckButtonForTest();
  ASSERT_TRUE(checkbox);
  const auto* const title_label =
      views::AsViewClass<views::Label>(view->GetViewByID(
          base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));
  ASSERT_TRUE(title_label);

  {
    // Tap on the checkbox. The action shouldn't be complete because there is no
    // network connection.
    GestureTapOn(checkbox);
    const auto [task_error_type, button_action_type] = error_future.Take();
    EXPECT_EQ(task_error_type,
              GlanceablesTasksErrorType::kCantMarkCompleteNoNetwork);
    EXPECT_EQ(button_action_type,
              GlanceablesErrorMessageView::ButtonActionType::kDismiss);
  }

  // No `STRIKE_THROUGH` style should be applied to the label.
  EXPECT_FALSE(view->GetCompletedForTest());
  EXPECT_FALSE(title_label->font_list().GetFontStyle() &
               gfx::Font::FontStyle::STRIKE_THROUGH);

  {
    // Clicking on the title label when no network connected will not show the
    // textfield.
    GestureTapOn(title_label);
    EXPECT_EQ(title_label, view->GetViewByID(base::to_underlying(
                               GlanceablesViewId::kTaskItemTitleLabel)));
    EXPECT_FALSE(view->GetViewByID(
        base::to_underlying(GlanceablesViewId::kTaskItemTitleTextField)));
    const auto [task_error_type, button_action_type] = error_future.Take();
    EXPECT_EQ(task_error_type,
              GlanceablesTasksErrorType::kCantUpdateTitleNoNetwork);
    EXPECT_EQ(button_action_type,
              GlanceablesErrorMessageView::ButtonActionType::kDismiss);
  }
}

TEST_F(GlanceablesTaskViewTest, InvokesMarkAsCompletedCallback) {
  const auto task = api::Task("task-id", "Task title",
                              /*due=*/std::nullopt, /*completed=*/false,
                              /*has_subtasks=*/false, /*has_email_link=*/false,
                              /*has_notes=*/false, /*updated=*/base::Time(),
                              /*web_view_link=*/GURL());

  base::test::TestFuture<const std::string&, bool> future;

  const auto widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  const auto* const view =
      widget->SetContentsView(std::make_unique<GlanceablesTaskView>(
          &task, /*mark_as_completed_callback=*/future.GetRepeatingCallback(),
          /*save_callback=*/base::DoNothing(),
          /*edit_in_browser_callback=*/base::DoNothing(),
          /*show_error_message_callback=*/base::DoNothing()));
  ASSERT_TRUE(view);

  EXPECT_FALSE(view->GetCompletedForTest());

  const auto* const checkbox = view->GetCheckButtonForTest();
  ASSERT_TRUE(checkbox);

  // Mark as completed by pressing `checkbox`.
  {
    GestureTapOn(checkbox);
    EXPECT_TRUE(view->GetCompletedForTest());
    const auto [task_id, completed] = future.Take();
    EXPECT_EQ(task_id, "task-id");
    EXPECT_TRUE(completed);
  }

  // Undo / mark as not completed by pressing `checkbox` again.
  {
    GestureTapOn(checkbox);
    EXPECT_FALSE(view->GetCompletedForTest());
    const auto [task_id, completed] = future.Take();
    EXPECT_EQ(task_id, "task-id");
    EXPECT_FALSE(completed);
  }
}

TEST_F(GlanceablesTaskViewTest, EntersAndExitsEditState) {
  const auto task = api::Task("task-id", "Task title",
                              /*due=*/std::nullopt, /*completed=*/false,
                              /*has_subtasks=*/false, /*has_email_link=*/false,
                              /*has_notes=*/false, /*updated=*/base::Time(),
                              /*web_view_link=*/GURL());

  const auto widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  const auto* const view =
      widget->SetContentsView(std::make_unique<GlanceablesTaskView>(
          &task, /*mark_as_completed_callback=*/base::DoNothing(),
          /*save_callback=*/base::DoNothing(),
          /*edit_in_browser_callback=*/base::DoNothing(),
          /*show_error_message_callback=*/base::DoNothing()));

  {
    const auto* const title_label =
        views::AsViewClass<views::Label>(view->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));
    const auto* const title_text_field =
        views::AsViewClass<views::Textfield>(view->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemTitleTextField)));

    ASSERT_TRUE(title_label);
    ASSERT_FALSE(title_text_field);
    EXPECT_EQ(title_label->GetText(), u"Task title");

    LeftClickOn(title_label);
  }

  {
    const auto* const title_label =
        views::AsViewClass<views::Label>(view->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));
    const auto* const title_text_field =
        views::AsViewClass<views::Textfield>(view->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemTitleTextField)));

    ASSERT_FALSE(title_label);
    ASSERT_TRUE(title_text_field);
    EXPECT_EQ(title_text_field->GetText(), u"Task title");

    PressAndReleaseKey(ui::VKEY_SPACE);
    PressAndReleaseKey(ui::VKEY_U);
    PressAndReleaseKey(ui::VKEY_P);
    PressAndReleaseKey(ui::VKEY_D);

    PressAndReleaseKey(ui::VKEY_ESCAPE);
    base::RunLoop().RunUntilIdle();
  }

  {
    const auto* const title_label =
        views::AsViewClass<views::Label>(view->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));
    const auto* const title_text_field =
        views::AsViewClass<views::Textfield>(view->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemTitleTextField)));

    ASSERT_TRUE(title_label);
    ASSERT_FALSE(title_text_field);
    EXPECT_EQ(title_label->GetText(), u"Task title upd");
  }
}

TEST_F(GlanceablesTaskViewTest, InvokesSaveCallbackAfterAdding) {
  base::test::TestFuture<base::WeakPtr<GlanceablesTaskView>, const std::string&,
                         const std::string&,
                         api::TasksClient::OnTaskSavedCallback>
      future;

  const auto widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  auto* const view =
      widget->SetContentsView(std::make_unique<GlanceablesTaskView>(
          /*task=*/nullptr, /*mark_as_completed_callback=*/base::DoNothing(),
          /*save_callback=*/future.GetRepeatingCallback(),
          /*edit_in_browser_callback=*/base::DoNothing(),
          /*show_error_message_callback=*/base::DoNothing()));
  ASSERT_TRUE(view);

  view->UpdateTaskTitleViewForState(
      GlanceablesTaskView::TaskTitleViewState::kEdit);
  PressAndReleaseKey(ui::VKEY_N, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::VKEY_E);
  PressAndReleaseKey(ui::VKEY_W);
  PressAndReleaseKey(ui::VKEY_ESCAPE);

  const auto [task_view, task_id, title, callback] = future.Take();
  EXPECT_TRUE(task_id.empty());
  EXPECT_EQ(title, "New");
}

TEST_F(GlanceablesTaskViewTest, InvokesSaveCallbackAfterEditing) {
  const auto task = api::Task("task-id", "Task title",
                              /*due=*/std::nullopt, /*completed=*/false,
                              /*has_subtasks=*/false, /*has_email_link=*/false,
                              /*has_notes=*/false, /*updated=*/base::Time(),
                              /*web_view_link=*/GURL());

  base::test::TestFuture<base::WeakPtr<GlanceablesTaskView>, const std::string&,
                         const std::string&,
                         api::TasksClient::OnTaskSavedCallback>
      future;

  const auto widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  auto* const view =
      widget->SetContentsView(std::make_unique<GlanceablesTaskView>(
          &task, /*mark_as_completed_callback=*/base::DoNothing(),
          /*save_callback=*/future.GetRepeatingCallback(),
          /*edit_in_browser_callback=*/base::DoNothing(),
          /*show_error_message_callback=*/base::DoNothing()));
  ASSERT_TRUE(view);

  view->UpdateTaskTitleViewForState(
      GlanceablesTaskView::TaskTitleViewState::kEdit);
  PressAndReleaseKey(ui::VKEY_SPACE);
  PressAndReleaseKey(ui::VKEY_U);
  PressAndReleaseKey(ui::VKEY_P);
  PressAndReleaseKey(ui::VKEY_D);
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  base::RunLoop().RunUntilIdle();

  const auto [task_view, task_id, title, callback] = future.Take();
  EXPECT_EQ(task_id, "task-id");
  EXPECT_EQ(title, "Task title upd");
}

TEST_F(GlanceablesTaskViewTest, CommitEditedTaskOnTab) {
  const auto task = api::Task("task-id", "Task title",
                              /*due=*/std::nullopt, /*completed=*/false,
                              /*has_subtasks=*/false, /*has_email_link=*/false,
                              /*has_notes=*/false, /*updated=*/base::Time(),
                              /*web_view_link=*/GURL());

  base::test::TestFuture<base::WeakPtr<GlanceablesTaskView>, const std::string&,
                         const std::string&,
                         api::TasksClient::OnTaskSavedCallback>
      future;

  const auto widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  auto* const view =
      widget->SetContentsView(std::make_unique<GlanceablesTaskView>(
          &task, /*mark_as_completed_callback=*/base::DoNothing(),
          /*save_callback=*/future.GetRepeatingCallback(),
          /*edit_in_browser_callback=*/base::DoNothing(),
          /*show_error_message_callback=*/base::DoNothing()));
  ASSERT_TRUE(view);

  view->UpdateTaskTitleViewForState(
      GlanceablesTaskView::TaskTitleViewState::kEdit);
  PressAndReleaseKey(ui::VKEY_SPACE);
  PressAndReleaseKey(ui::VKEY_U);
  PressAndReleaseKey(ui::VKEY_P);
  PressAndReleaseKey(ui::VKEY_D);

  PressAndReleaseKey(ui::VKEY_TAB);
  base::RunLoop().RunUntilIdle();

  {
    auto [task_view, task_id, title, callback] = future.Take();
    EXPECT_EQ(task_id, "task-id");
    EXPECT_EQ(title, "Task title upd");
    const auto updated_task =
        api::Task("task-id", "New upd",
                  /*due=*/std::nullopt, /*completed=*/false,
                  /*has_subtasks=*/false,
                  /*has_email_link=*/false, /*has_notes=*/false,
                  /*updated=*/base::Time::Now(), /*web_view_link=*/GURL());
    std::move(callback).Run(&updated_task);
  }

  EXPECT_FALSE(views::AsViewClass<views::Label>(view->GetViewByID(
      base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel))));
  EXPECT_TRUE(views::AsViewClass<views::Textfield>(view->GetViewByID(
      base::to_underlying(GlanceablesViewId::kTaskItemTitleTextField))));
  const auto* edit_in_browser_button = view->GetViewByID(
      base::to_underlying(GlanceablesViewId::kTaskItemEditInBrowserLabel));
  ASSERT_TRUE(edit_in_browser_button);
  EXPECT_TRUE(edit_in_browser_button->HasFocus());

  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  base::RunLoop().RunUntilIdle();

  PressAndReleaseKey(ui::VKEY_RIGHT);
  PressAndReleaseKey(ui::VKEY_A);

  PressAndReleaseKey(ui::VKEY_TAB);
  base::RunLoop().RunUntilIdle();

  {
    const auto [task_view, task_id, title, callback] = future.Take();
    EXPECT_EQ(task_id, "task-id");
    EXPECT_EQ(title, "Task title upda");
  }

  edit_in_browser_button = view->GetViewByID(
      base::to_underlying(GlanceablesViewId::kTaskItemEditInBrowserLabel));
  ASSERT_TRUE(edit_in_browser_button);
  EXPECT_TRUE(edit_in_browser_button->HasFocus());

  view->GetFocusManager()->ClearFocus();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(views::AsViewClass<views::Label>(view->GetViewByID(
      base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel))));
  EXPECT_FALSE(views::AsViewClass<views::Textfield>(view->GetViewByID(
      base::to_underlying(GlanceablesViewId::kTaskItemTitleTextField))));
  EXPECT_FALSE(views::AsViewClass<views::Textfield>(view->GetViewByID(
      base::to_underlying(GlanceablesViewId::kTaskItemEditInBrowserLabel))));
}

TEST_F(GlanceablesTaskViewTest, SupportsEditingRightAfterAdding) {
  base::test::TestFuture<base::WeakPtr<GlanceablesTaskView>, const std::string&,
                         const std::string&,
                         api::TasksClient::OnTaskSavedCallback>
      future;

  const auto widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  auto* const view =
      widget->SetContentsView(std::make_unique<GlanceablesTaskView>(
          /*task=*/nullptr, /*mark_as_completed_callback=*/base::DoNothing(),
          /*save_callback=*/future.GetRepeatingCallback(),
          /*edit_in_browser_callback=*/base::DoNothing(),
          /*show_error_message_callback=*/base::DoNothing()));
  ASSERT_TRUE(view);

  {
    view->UpdateTaskTitleViewForState(
        GlanceablesTaskView::TaskTitleViewState::kEdit);
    PressAndReleaseKey(ui::VKEY_N, ui::EF_SHIFT_DOWN);
    PressAndReleaseKey(ui::VKEY_E);
    PressAndReleaseKey(ui::VKEY_W);
    PressAndReleaseKey(ui::VKEY_ESCAPE);

    // Verify that `task_id` is empty after adding a task.
    auto [task_view, task_id, title, callback] = future.Take();
    EXPECT_TRUE(task_id.empty());
    EXPECT_EQ(title, "New");

    // Simulate reply, the view should update itself with the new task id.
    const auto created_task =
        api::Task("task-id", "New",
                  /*due=*/std::nullopt, /*completed=*/false,
                  /*has_subtasks=*/false,
                  /*has_email_link=*/false, /*has_notes=*/false,
                  /*updated=*/base::Time::Now(), /*web_view_link=*/GURL());
    std::move(callback).Run(&created_task);
  }

  {
    view->UpdateTaskTitleViewForState(
        GlanceablesTaskView::TaskTitleViewState::kEdit);
    PressAndReleaseKey(ui::VKEY_SPACE);
    PressAndReleaseKey(ui::VKEY_1);
    PressAndReleaseKey(ui::VKEY_ESCAPE);

    // Verify that `task_id` equals to "task-id" after editing the same task.
    const auto [task_view, task_id, title, callback] = future.Take();
    EXPECT_EQ(task_id, "task-id");
    EXPECT_EQ(title, "New 1");
  }
}

TEST_F(GlanceablesTaskViewTest, HandlesPressingCheckButtonWhileAdding) {
  base::test::TestFuture<base::WeakPtr<GlanceablesTaskView>, const std::string&,
                         const std::string&,
                         api::TasksClient::OnTaskSavedCallback>
      future;

  const auto widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  auto* const view =
      widget->SetContentsView(std::make_unique<GlanceablesTaskView>(
          /*task=*/nullptr, /*mark_as_completed_callback=*/base::DoNothing(),
          /*save_callback=*/future.GetRepeatingCallback(),
          /*edit_in_browser_callback=*/base::DoNothing(),
          /*show_error_message_callback=*/base::DoNothing()));
  ASSERT_TRUE(view);

  view->UpdateTaskTitleViewForState(
      GlanceablesTaskView::TaskTitleViewState::kEdit);
  EXPECT_FALSE(view->GetCompletedForTest());

  PressAndReleaseKey(ui::VKEY_N, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::VKEY_E);
  PressAndReleaseKey(ui::VKEY_W);

  // Tapping the disabled check button implicitly leads to committing task's
  // title, but this shouldn't change checked state or cause a crash.
  LeftClickOn(view->GetCheckButtonForTest());
  auto [task_view, task_id, title, callback] = future.Take();
  EXPECT_TRUE(task_id.empty());
  EXPECT_EQ(title, "New");
  EXPECT_FALSE(view->GetCompletedForTest());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(view->GetCompletedForTest());

  const auto* const title_label =
      views::AsViewClass<views::Label>(view->GetViewByID(
          base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));
  ASSERT_TRUE(title_label);
  const auto* const title_button =
      views::AsViewClass<views::LabelButton>(title_label->parent());
  ASSERT_TRUE(title_button);
  EXPECT_FALSE(title_button->GetEnabled());

  // Simulate reply, this should re-enable the checkbox and title buttons.
  const auto created_task =
      api::Task("task-id", "New",
                /*due=*/std::nullopt, /*completed=*/false,
                /*has_subtasks=*/false,
                /*has_email_link=*/false, /*has_notes=*/false,
                /*updated=*/base::Time::Now(), /*web_view_link=*/GURL());
  std::move(callback).Run(&created_task);
  EXPECT_TRUE(view->GetCheckButtonForTest()->GetEnabled());
  EXPECT_TRUE(title_button->GetEnabled());
}

}  // namespace ash
