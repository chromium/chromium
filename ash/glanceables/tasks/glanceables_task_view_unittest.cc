// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/tasks/glanceables_task_view.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/api/tasks/tasks_client.h"
#include "ash/api/tasks/tasks_types.h"
#include "ash/glanceables/common/glanceables_util.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/types/cxx23_to_underlying.h"
#include "chromeos/ash/components/settings/scoped_timezone_settings.h"
#include "google_apis/common/api_error_codes.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {

class GlanceablesTaskViewTest : public AshTestBase {
 public:
  GlanceablesTaskViewTest() {
    glanceables_util::SetIsNetworkConnectedForTest(true);
  }
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
                                /*updated=*/due, /*web_view_link=*/GURL(),
                                api::Task::OriginSurfaceType::kRegular);
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
                              /*web_view_link=*/GURL(),
                              api::Task::OriginSurfaceType::kRegular);

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
  EXPECT_TRUE(title_label->IsDrawn());

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
                              /*web_view_link=*/GURL(),
                              api::Task::OriginSurfaceType::kRegular);

  const auto widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  base::test::TestFuture<GlanceablesTasksErrorType,
                         ErrorMessageToast::ButtonActionType>
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
  EXPECT_TRUE(title_label->IsDrawn());

  {
    // Tap on the checkbox. The action shouldn't be complete because there is no
    // network connection.
    GestureTapOn(checkbox);
    const auto [task_error_type, button_action_type] = error_future.Take();
    EXPECT_EQ(task_error_type,
              GlanceablesTasksErrorType::kCantMarkCompleteNoNetwork);
    EXPECT_EQ(button_action_type,
              ErrorMessageToast::ButtonActionType::kDismiss);
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
    const auto* title_text_field = view->GetViewByID(
        base::to_underlying(GlanceablesViewId::kTaskItemTitleTextField));
    EXPECT_FALSE(title_text_field);
    const auto [task_error_type, button_action_type] = error_future.Take();
    EXPECT_EQ(task_error_type,
              GlanceablesTasksErrorType::kCantUpdateTitleNoNetwork);
    EXPECT_EQ(button_action_type,
              ErrorMessageToast::ButtonActionType::kDismiss);
  }
}

TEST_F(GlanceablesTaskViewTest, InvokesMarkAsCompletedCallback) {
  const auto task = api::Task("task-id", "Task title",
                              /*due=*/std::nullopt, /*completed=*/false,
                              /*has_subtasks=*/false, /*has_email_link=*/false,
                              /*has_notes=*/false, /*updated=*/base::Time(),
                              /*web_view_link=*/GURL(),
                              api::Task::OriginSurfaceType::kRegular);

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
                              /*web_view_link=*/GURL(),
                              api::Task::OriginSurfaceType::kRegular);

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
    EXPECT_TRUE(title_label->IsDrawn());
    EXPECT_EQ(title_label->GetText(), u"Task title");

    EXPECT_FALSE(title_text_field);

    LeftClickOn(title_label);
  }

  {
    const auto* const title_label =
        views::AsViewClass<views::Label>(view->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));
    const auto* const title_text_field =
        views::AsViewClass<views::Textfield>(view->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemTitleTextField)));

    EXPECT_FALSE(title_label);

    ASSERT_TRUE(title_text_field);
    EXPECT_TRUE(title_text_field->IsDrawn());
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
    EXPECT_TRUE(title_label->IsDrawn());
    EXPECT_EQ(title_label->GetText(), u"Task title upd");
    EXPECT_FALSE(title_text_field);
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
                              /*web_view_link=*/GURL(),
                              api::Task::OriginSurfaceType::kRegular);

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
                              /*web_view_link=*/GURL(),
                              api::Task::OriginSurfaceType::kRegular);

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
                  /*updated=*/base::Time::Now(), /*web_view_link=*/GURL(),
                  api::Task::OriginSurfaceType::kRegular);
    std::move(callback).Run(google_apis::ApiErrorCode::HTTP_SUCCESS,
                            &updated_task);
  }

  const auto* title_label = views::AsViewClass<views::Label>(view->GetViewByID(
      base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));
  EXPECT_FALSE(title_label);
  const auto* title_text_field =
      views::AsViewClass<views::Textfield>(view->GetViewByID(
          base::to_underlying(GlanceablesViewId::kTaskItemTitleTextField)));
  ASSERT_TRUE(title_text_field);
  EXPECT_TRUE(title_text_field->IsDrawn());
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

  title_label = views::AsViewClass<views::Label>(view->GetViewByID(
      base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));
  ASSERT_TRUE(title_label);
  EXPECT_TRUE(title_label->IsDrawn());
  title_text_field = views::AsViewClass<views::Textfield>(view->GetViewByID(
      base::to_underlying(GlanceablesViewId::kTaskItemTitleTextField)));
  EXPECT_FALSE(title_text_field);
  edit_in_browser_button = view->GetViewByID(
      base::to_underlying(GlanceablesViewId::kTaskItemEditInBrowserLabel));
  EXPECT_FALSE(edit_in_browser_button);
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
                  /*updated=*/base::Time::Now(), /*web_view_link=*/GURL(),
                  api::Task::OriginSurfaceType::kRegular);
    std::move(callback).Run(google_apis::ApiErrorCode::HTTP_SUCCESS,
                            &created_task);
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
  EXPECT_TRUE(title_label->IsDrawn());
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
                /*updated=*/base::Time::Now(), /*web_view_link=*/GURL(),
                api::Task::OriginSurfaceType::kRegular);
  std::move(callback).Run(google_apis::ApiErrorCode::HTTP_SUCCESS,
                          &created_task);
  EXPECT_TRUE(view->GetCheckButtonForTest()->GetEnabled());
  EXPECT_TRUE(title_button->GetEnabled());
}

TEST_F(GlanceablesTaskViewTest, DisplaysOriginSurfaceType) {
  for (auto origin_surface_type : {api::Task::OriginSurfaceType::kRegular,
                                   api::Task::OriginSurfaceType::kDocument,
                                   api::Task::OriginSurfaceType::kSpace,
                                   api::Task::OriginSurfaceType::kUnknown}) {
    SCOPED_TRACE(::testing::Message()
                 << "origin_surface_type="
                 << base::to_underlying(origin_surface_type));

    const auto task = api::Task("task-id", "Task title",
                                /*due=*/std::nullopt, /*completed=*/false,
                                /*has_subtasks=*/false,
                                /*has_email_link=*/false,
                                /*has_notes=*/false,
                                /*updated=*/base::Time::Now(),
                                /*web_view_link=*/GURL(), origin_surface_type);
    const auto widget = CreateFramelessTestWidget();
    widget->SetFullscreen(true);
    const auto* const view =
        widget->SetContentsView(std::make_unique<GlanceablesTaskView>(
            &task, /*mark_as_completed_callback=*/base::DoNothing(),
            /*save_callback=*/base::DoNothing(),
            /*edit_in_browser_callback=*/base::DoNothing(),
            /*show_error_message_callback=*/base::DoNothing()));

    const auto* const origin_surface_type_icon =
        views::AsViewClass<views::ImageView>(view->GetViewByID(
            base::to_underlying(GlanceablesViewId::kOriginSurfaceTypeIcon)));

    // Check presence of the origin surface type icon. It's only added to
    // assigned/shared tasks except `kUnknown` and visible by default.
    if (origin_surface_type == api::Task::OriginSurfaceType::kDocument ||
        origin_surface_type == api::Task::OriginSurfaceType::kSpace) {
      ASSERT_TRUE(origin_surface_type_icon);
      EXPECT_TRUE(origin_surface_type_icon->GetVisible());
      EXPECT_FALSE(views::AsViewClass<views::View>(view->GetViewByID(
          base::to_underlying(GlanceablesViewId::kAssignedTaskNotice))));
    } else {
      EXPECT_FALSE(origin_surface_type_icon);
    }

    {
      const auto* const title_label =
          views::AsViewClass<views::Label>(view->GetViewByID(
              base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));
      ASSERT_TRUE(title_label);
      LeftClickOn(title_label);
    }

    // The icon should disappear after switching to edit mode...
    if (origin_surface_type == api::Task::OriginSurfaceType::kDocument ||
        origin_surface_type == api::Task::OriginSurfaceType::kSpace) {
      EXPECT_FALSE(origin_surface_type_icon->GetVisible());
    }

    // ...and the notice should appear for all tasks except `kRegular`.
    if (origin_surface_type != api::Task::OriginSurfaceType::kRegular) {
      EXPECT_TRUE(views::AsViewClass<views::View>(view->GetViewByID(
          base::to_underlying(GlanceablesViewId::kAssignedTaskNotice))));
    }
  }
}

TEST_F(GlanceablesTaskViewTest,
       CheckButtonAccessibleDefaultActionVerbAndCheckedState) {
  const auto task = api::Task("task-id", "Task title",
                              /*due=*/std::nullopt, /*completed=*/false,
                              /*has_subtasks=*/false, /*has_email_link=*/false,
                              /*has_notes=*/false, /*updated=*/base::Time(),
                              /*web_view_link=*/GURL(),
                              api::Task::OriginSurfaceType::kRegular);
  const auto widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  auto* view = widget->SetContentsView(std::make_unique<GlanceablesTaskView>(
      &task, /*mark_as_completed_callback=*/base::DoNothing(),
      /*save_callback=*/base::DoNothing(),
      /*edit_in_browser_callback=*/base::DoNothing(),
      /*show_error_message_callback=*/base::DoNothing()));
  auto* check_button = view->GetCheckButtonForTest();
  ui::AXNodeData data;
  check_button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetDefaultActionVerb(), ax::mojom::DefaultActionVerb::kCheck);

  view->SetCheckedForTest(true);
  data = ui::AXNodeData();
  check_button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kTrue);
  EXPECT_EQ(data.GetDefaultActionVerb(),
            ax::mojom::DefaultActionVerb::kUncheck);

  view->SetCheckedForTest(false);
  data = ui::AXNodeData();
  check_button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kFalse);
  EXPECT_EQ(data.GetDefaultActionVerb(), ax::mojom::DefaultActionVerb::kCheck);
}

}  // namespace ash
