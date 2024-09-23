// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/classroom/glanceables_classroom_item_view.h"

#include <optional>
#include <string>
#include <vector>

#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/types/cxx23_to_underlying.h"
#include "chromeos/ash/components/settings/scoped_timezone_settings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

namespace ash {

class GlanceablesClassroomItemViewTest : public AshTestBase {
 public:
  const views::ImageView* GetIconView(
      const GlanceablesClassroomItemView& view) const {
    return views::AsViewClass<views::ImageView>(view.GetViewByID(
        base::to_underlying(GlanceablesViewId::kClassroomItemIcon)));
  }

  const views::Label* GetCourseWorkTitleLabel(
      const GlanceablesClassroomItemView& view) const {
    return views::AsViewClass<views::Label>(
        view.GetViewByID(base::to_underlying(
            GlanceablesViewId::kClassroomItemCourseWorkTitleLabel)));
  }

  const views::Label* GetCourseTitleLabel(
      const GlanceablesClassroomItemView& view) const {
    return views::AsViewClass<views::Label>(
        view.GetViewByID(base::to_underlying(
            GlanceablesViewId::kClassroomItemCourseTitleLabel)));
  }

  const views::Label* GetDueDateLabel(
      const GlanceablesClassroomItemView& view) const {
    return views::AsViewClass<views::Label>(view.GetViewByID(
        base::to_underlying(GlanceablesViewId::kClassroomItemDueDateLabel)));
  }

  const views::Label* GetDueTimeLabel(
      const GlanceablesClassroomItemView& view) const {
    return views::AsViewClass<views::Label>(view.GetViewByID(
        base::to_underlying(GlanceablesViewId::kClassroomItemDueTimeLabel)));
  }
};

TEST_F(GlanceablesClassroomItemViewTest, RendersWithoutDueDateTime) {
  const auto assignment = GlanceablesClassroomAssignment(
      "Algebra", "Solve equation",
      GURL("https://classroom.google.com/test-link-1"), std::nullopt,
      base::Time(), std::nullopt);
  const auto view =
      GlanceablesClassroomItemView(&assignment, base::DoNothing());

  const auto* const icon_view = GetIconView(view);
  const auto* const course_work_title_label = GetCourseWorkTitleLabel(view);
  const auto* const course_title_label = GetCourseTitleLabel(view);
  const auto* const due_date_label = GetDueDateLabel(view);
  const auto* const due_time_label = GetDueTimeLabel(view);

  ASSERT_TRUE(icon_view);
  ASSERT_TRUE(course_work_title_label);
  ASSERT_TRUE(course_title_label);
  EXPECT_FALSE(due_date_label);
  EXPECT_FALSE(due_time_label);

  EXPECT_EQ(course_work_title_label->GetText(), u"Solve equation");
  EXPECT_EQ(course_title_label->GetText(), u"Algebra");
}

TEST_F(GlanceablesClassroomItemViewTest, RendersWithDueDateTime) {
  // "Now" equals to Sunday, 4/9/2023 5:15pm in the overridden timezone.
  base::subtle::ScopedTimeClockOverrides time_override(
      []() {
        base::Time now;
        EXPECT_TRUE(base::Time::FromString("10 Apr 2023 00:15 GMT", &now));
        return now;
      },
      nullptr, nullptr);
  ash::system::ScopedTimezoneSettings tz(u"America/Los_Angeles");

  const std::vector<std::u16string> expected_due_date_labels{
      u"Apr 8", u"Today", u"Mon", u"Tue",   u"Wed",
      u"Thu",   u"Fri",   u"Sat", u"Apr 16"};

  // For 9 subsequent assignments starting from "yesterday", check their due
  // date and time labels.
  base::Time due = base::Time::Now() - base::Days(1);
  for (size_t i = 0; i < 9; ++i) {
    const auto assignment = GlanceablesClassroomAssignment(
        "Algebra", "Solve equation",
        GURL("https://classroom.google.com/test-link-1"), due, base::Time(),
        std::nullopt);
    const auto view =
        GlanceablesClassroomItemView(&assignment, base::DoNothing());

    const auto* const due_date_label = GetDueDateLabel(view);
    const auto* const due_time_label = GetDueTimeLabel(view);

    EXPECT_EQ(due_date_label->GetText(), expected_due_date_labels.at(i));
    EXPECT_EQ(due_time_label->GetText(), u"5:15\x202FPM");

    due += base::Days(1);
  }
}

TEST_F(GlanceablesClassroomItemViewTest, RendersDueTimeIn24HrFormat) {
  ash::system::ScopedTimezoneSettings tz(u"America/Los_Angeles");
  Shell::Get()->system_tray_model()->SetUse24HourClock(true);

  base::Time due;
  ASSERT_TRUE(base::Time::FromString("10 Apr 2023 00:15 GMT", &due));

  const auto assignment = GlanceablesClassroomAssignment(
      "Algebra", "Solve equation",
      GURL("https://classroom.google.com/test-link-1"), due, base::Time(),
      std::nullopt);
  const auto view =
      GlanceablesClassroomItemView(&assignment, base::DoNothing());
  const auto* const due_time_label = GetDueTimeLabel(view);

  ASSERT_TRUE(due_time_label);
  EXPECT_EQ(due_time_label->GetText(), u"17:15");
}

TEST_F(GlanceablesClassroomItemViewTest, DoesNotRenderDueTimeFor2359) {
  // 1 - for ICU formatters; 2 - for `base::Time::LocalExplode`.
  ash::system::ScopedTimezoneSettings tz(u"America/Los_Angeles");
  calendar_test_utils::ScopedLibcTimeZone libc_tz("America/Los_Angeles");

  base::Time due;
  ASSERT_TRUE(base::Time::FromString("10 Apr 2023 06:59 GMT", &due));

  const auto assignment = GlanceablesClassroomAssignment(
      "Algebra", "Solve equation",
      GURL("https://classroom.google.com/test-link-1"), due, base::Time(),
      std::nullopt);
  const auto view =
      GlanceablesClassroomItemView(&assignment, base::DoNothing());
  const auto* const due_time_label = GetDueTimeLabel(view);

  ASSERT_TRUE(due_time_label);
  EXPECT_TRUE(due_time_label->GetText().empty());
}

TEST_F(GlanceablesClassroomItemViewTest, AccessibleProperties) {
  const auto assignment = GlanceablesClassroomAssignment(
      "Algebra", "Solve equation",
      GURL("https://classroom.google.com/test-link-1"), std::nullopt,
      base::Time(), std::nullopt);
  auto view = GlanceablesClassroomItemView(&assignment, base::DoNothing());
  ui::AXNodeData data;

  view.GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kListItem);
  EXPECT_EQ(data.GetDefaultActionVerb(), ax::mojom::DefaultActionVerb::kClick);

  view.SetEnabled(false);
  data = ui::AXNodeData();
  view.GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetDefaultActionVerb(), ax::mojom::DefaultActionVerb::kClick);
}

}  // namespace ash
