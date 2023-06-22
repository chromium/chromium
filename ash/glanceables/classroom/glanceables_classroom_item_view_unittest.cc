// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/classroom/glanceables_classroom_item_view.h"

#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/time/time.h"
#include "chromeos/ash/components/settings/scoped_timezone_settings.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

namespace ash {

class GlanceablesClassroomItemViewTest : public AshTestBase {
 public:
  const views::ImageView* GetIconView(
      const GlanceablesClassroomItemView& view) const {
    return views::AsViewClass<views::ImageView>(
        view.GetViewByID(GlanceablesClassroomItemView::kIconViewId));
  }

  const views::Label* GetCourseWorkTitleLabel(
      const GlanceablesClassroomItemView& view) const {
    return views::AsViewClass<views::Label>(view.GetViewByID(
        GlanceablesClassroomItemView::kCourseWorkTitleLabelId));
  }

  const views::Label* GetCourseTitleLabel(
      const GlanceablesClassroomItemView& view) const {
    return views::AsViewClass<views::Label>(
        view.GetViewByID(GlanceablesClassroomItemView::kCourseTitleLabelId));
  }

  const views::Label* GetDueDateLabel(
      const GlanceablesClassroomItemView& view) const {
    return views::AsViewClass<views::Label>(
        view.GetViewByID(GlanceablesClassroomItemView::kDueDateLabelId));
  }

  const views::Label* GetDueTimeLabel(
      const GlanceablesClassroomItemView& view) const {
    return views::AsViewClass<views::Label>(
        view.GetViewByID(GlanceablesClassroomItemView::kDueTimeLabelId));
  }
};

TEST_F(GlanceablesClassroomItemViewTest, RendersWithoutDueDateTime) {
  const auto assignment = GlanceablesClassroomStudentAssignment(
      "Algebra", "Solve equation",
      GURL("https://classroom.google.com/test-link-1"), absl::nullopt);
  const auto view = GlanceablesClassroomItemView(&assignment);

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
  ash::system::ScopedTimezoneSettings tz(u"America/Los_Angeles");

  base::Time due;
  ASSERT_TRUE(base::Time::FromString("10 Apr 2023 00:15 GMT", &due));

  const auto assignment = GlanceablesClassroomStudentAssignment(
      "Algebra", "Solve equation",
      GURL("https://classroom.google.com/test-link-1"), due);
  const auto view = GlanceablesClassroomItemView(&assignment);

  const auto* const icon_view = GetIconView(view);
  const auto* const course_work_title_label = GetCourseWorkTitleLabel(view);
  const auto* const course_title_label = GetCourseTitleLabel(view);
  const auto* const due_date_label = GetDueDateLabel(view);
  const auto* const due_time_label = GetDueTimeLabel(view);

  ASSERT_TRUE(icon_view);
  ASSERT_TRUE(course_work_title_label);
  ASSERT_TRUE(course_title_label);
  ASSERT_TRUE(due_date_label);
  ASSERT_TRUE(due_time_label);

  EXPECT_EQ(course_work_title_label->GetText(), u"Solve equation");
  EXPECT_EQ(course_title_label->GetText(), u"Algebra");
  EXPECT_EQ(due_date_label->GetText(), u"Today");
  EXPECT_EQ(due_time_label->GetText(), u"5:15\x202FPM");
}

TEST_F(GlanceablesClassroomItemViewTest, RendersDueTimeIn24HrFormat) {
  ash::system::ScopedTimezoneSettings tz(u"America/Los_Angeles");
  Shell::Get()->system_tray_model()->SetUse24HourClock(true);

  base::Time due;
  ASSERT_TRUE(base::Time::FromString("10 Apr 2023 00:15 GMT", &due));

  const auto assignment = GlanceablesClassroomStudentAssignment(
      "Algebra", "Solve equation",
      GURL("https://classroom.google.com/test-link-1"), due);
  const auto view = GlanceablesClassroomItemView(&assignment);
  const auto* const due_time_label = GetDueTimeLabel(view);

  ASSERT_TRUE(due_time_label);
  EXPECT_EQ(due_time_label->GetText(), u"17:15");
}

}  // namespace ash
