// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/tasks/glanceables_task_view.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/tasks/glanceables_tasks_types.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/types/cxx23_to_underlying.h"
#include "chromeos/ash/components/settings/scoped_timezone_settings.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
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

    const auto task = GlanceablesTask(
        "task-id", "Task title", /*completed=*/false,
        /*due=*/due,
        /*has_subtasks=*/false, /*has_email_link=*/false, /*has_notes=*/false);
    const auto view = GlanceablesTaskView("task-list-id", &task);

    const auto* const due_label =
        views::AsViewClass<views::Label>(view.GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemDueLabel)));
    ASSERT_TRUE(due_label);

    EXPECT_EQ(due_label->GetText(), tc.expected_text);
  }
}

TEST_F(GlanceablesTaskViewTest, EntersAndExitsEditState) {
  base::test::ScopedFeatureList features{features::kGlanceablesV2TasksAddEdit};

  const auto task = GlanceablesTask(
      "task-id", "Task title", /*completed=*/false,
      /*due=*/absl::nullopt,
      /*has_subtasks=*/false, /*has_email_link=*/false, /*has_notes=*/false);

  const auto widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  const auto* const view = widget->SetContentsView(
      std::make_unique<GlanceablesTaskView>("task-list-id", &task));

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

}  // namespace ash
