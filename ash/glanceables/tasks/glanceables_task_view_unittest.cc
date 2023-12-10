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
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback_helpers.h"
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
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

using GlanceablesTaskViewTest = AshTestBase;

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

    const auto task = api::Task("task-id", "Task title", /*completed=*/false,
                                /*due=*/due,
                                /*has_subtasks=*/false,
                                /*has_email_link=*/false, /*has_notes=*/false,
                                /*updated=*/due);
    const auto view = GlanceablesTaskView(
        &task, /*mark_as_completed_callback=*/base::DoNothing(),
        /*save_callback=*/base::DoNothing());

    const auto* const due_label =
        views::AsViewClass<views::Label>(view.GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemDueLabel)));
    ASSERT_TRUE(due_label);

    EXPECT_EQ(due_label->GetText(), tc.expected_text);
  }
}

TEST_F(GlanceablesTaskViewTest, EntersAndExitsEditState) {
  base::test::ScopedFeatureList features{
      features::kGlanceablesTimeManagementStableLaunch};

  const auto task = api::Task("task-id", "Task title", /*completed=*/false,
                              /*due=*/std::nullopt,
                              /*has_subtasks=*/false, /*has_email_link=*/false,
                              /*has_notes=*/false, /*updated=*/base::Time());

  const auto widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  const auto* const view =
      widget->SetContentsView(std::make_unique<GlanceablesTaskView>(
          &task, /*mark_as_completed_callback=*/base::DoNothing(),
          /*save_callback=*/base::DoNothing()));

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

TEST_F(GlanceablesTaskViewTest,
       AppliesStrikeThroughStyleAfterMarkingAsComplete) {
  const auto task = api::Task("task-id", "Task title", /*completed=*/false,
                              /*due=*/std::nullopt,
                              /*has_subtasks=*/false, /*has_email_link=*/false,
                              /*has_notes=*/false, /*updated=*/base::Time());

  const auto widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  const auto* const view =
      widget->SetContentsView(std::make_unique<GlanceablesTaskView>(
          &task, /*mark_as_completed_callback=*/base::DoNothing(),
          /*save_callback=*/base::DoNothing()));
  ASSERT_TRUE(view);

  const auto* const checkbox = view->GetButtonForTest();
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

TEST_F(GlanceablesTaskViewTest, InvokesMarkAsCompletedCallback) {
  const auto task = api::Task("task-id", "Task title", /*completed=*/false,
                              /*due=*/std::nullopt,
                              /*has_subtasks=*/false, /*has_email_link=*/false,
                              /*has_notes=*/false, /*updated=*/base::Time());

  base::test::TestFuture<const std::string&, bool> future;

  const auto widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  const auto* const view =
      widget->SetContentsView(std::make_unique<GlanceablesTaskView>(
          &task, /*mark_as_completed_callback=*/future.GetRepeatingCallback(),
          /*save_callback=*/base::DoNothing()));
  ASSERT_TRUE(view);

  EXPECT_FALSE(view->GetCompletedForTest());

  const auto* const checkbox = view->GetButtonForTest();
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

TEST_F(GlanceablesTaskViewTest, InvokesSaveCallbackAfterAdding) {
  base::test::TestFuture<const std::string&, const std::string&,
                         api::TasksClient::OnTaskSavedCallback>
      future;

  const auto widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  auto* const view =
      widget->SetContentsView(std::make_unique<GlanceablesTaskView>(
          /*task=*/nullptr, /*mark_as_completed_callback=*/base::DoNothing(),
          /*save_callback=*/future.GetRepeatingCallback()));
  ASSERT_TRUE(view);

  view->UpdateTaskTitleViewForState(
      GlanceablesTaskView::TaskTitleViewState::kEdit);
  PressAndReleaseKey(ui::VKEY_N, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::VKEY_E);
  PressAndReleaseKey(ui::VKEY_W);
  PressAndReleaseKey(ui::VKEY_ESCAPE);

  const auto [task_id, title, callback] = future.Take();
  EXPECT_TRUE(task_id.empty());
  EXPECT_EQ(title, "New");
}

TEST_F(GlanceablesTaskViewTest, InvokesSaveCallbackAfterEditing) {
  const auto task = api::Task("task-id", "Task title", /*completed=*/false,
                              /*due=*/std::nullopt,
                              /*has_subtasks=*/false, /*has_email_link=*/false,
                              /*has_notes=*/false, /*updated=*/base::Time());

  base::test::TestFuture<const std::string&, const std::string&,
                         api::TasksClient::OnTaskSavedCallback>
      future;

  const auto widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  auto* const view =
      widget->SetContentsView(std::make_unique<GlanceablesTaskView>(
          &task, /*mark_as_completed_callback=*/base::DoNothing(),
          /*save_callback=*/future.GetRepeatingCallback()));
  ASSERT_TRUE(view);

  view->UpdateTaskTitleViewForState(
      GlanceablesTaskView::TaskTitleViewState::kEdit);
  PressAndReleaseKey(ui::VKEY_SPACE);
  PressAndReleaseKey(ui::VKEY_U);
  PressAndReleaseKey(ui::VKEY_P);
  PressAndReleaseKey(ui::VKEY_D);
  PressAndReleaseKey(ui::VKEY_ESCAPE);

  const auto [task_id, title, callback] = future.Take();
  EXPECT_EQ(task_id, "task-id");
  EXPECT_EQ(title, "Task title upd");
}

TEST_F(GlanceablesTaskViewTest, SupportsEditingRightAfterAdding) {
  base::test::TestFuture<const std::string&, const std::string&,
                         api::TasksClient::OnTaskSavedCallback>
      future;

  const auto widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  auto* const view =
      widget->SetContentsView(std::make_unique<GlanceablesTaskView>(
          /*task=*/nullptr, /*mark_as_completed_callback=*/base::DoNothing(),
          /*save_callback=*/future.GetRepeatingCallback()));
  ASSERT_TRUE(view);

  {
    view->UpdateTaskTitleViewForState(
        GlanceablesTaskView::TaskTitleViewState::kEdit);
    PressAndReleaseKey(ui::VKEY_N, ui::EF_SHIFT_DOWN);
    PressAndReleaseKey(ui::VKEY_E);
    PressAndReleaseKey(ui::VKEY_W);
    PressAndReleaseKey(ui::VKEY_ESCAPE);

    // Verify that `task_id` is empty after adding a task.
    auto [task_id, title, callback] = future.Take();
    EXPECT_TRUE(task_id.empty());
    EXPECT_EQ(title, "New");

    // Simulate reply, the view should update itself with the new task id.
    const auto created_task =
        api::Task("task-id", "New", /*completed=*/false,
                  /*due=*/absl::nullopt,
                  /*has_subtasks=*/false,
                  /*has_email_link=*/false, /*has_notes=*/false,
                  /*updated=*/base::Time::Now());
    std::move(callback).Run(&created_task);
  }

  {
    view->UpdateTaskTitleViewForState(
        GlanceablesTaskView::TaskTitleViewState::kEdit);
    PressAndReleaseKey(ui::VKEY_SPACE);
    PressAndReleaseKey(ui::VKEY_1);
    PressAndReleaseKey(ui::VKEY_ESCAPE);

    // Verify that `task_id` equals to "task-id" after editing the same task.
    const auto [task_id, title, callback] = future.Take();
    EXPECT_EQ(task_id, "task-id");
    EXPECT_EQ(title, "New 1");
  }
}

}  // namespace ash
