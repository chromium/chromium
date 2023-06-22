// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/classroom/glanceables_classroom_item_view.h"

#include <memory>

#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/style/typography.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_utils.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

// Styles for the whole `GlanceablesClassroomItemView`.
constexpr int kBackgroundRadius = 4;
constexpr auto kInteriorMargin = gfx::Insets::VH(8, 0);

// Styles for the icon view.
constexpr int kIconViewBackgroundRadius = 16;
constexpr auto kIconViewMargin = gfx::Insets::VH(0, 12);
constexpr auto kIconViewPreferredSize =
    gfx::Size(kIconViewBackgroundRadius * 2, kIconViewBackgroundRadius * 2);
constexpr int kIconSize = 20;

// Styles for the container containing due date and time labels.
constexpr auto kDueLabelsMargin = gfx::Insets::VH(0, 16);

std::u16string GetFormattedDueDate(const base::Time& due) {
  // TODO(b/283370862): return "Today" / day of week / formatted date label.
  return u"Today";
}

std::u16string GetFormattedDueTime(const base::Time& due) {
  const bool use_12_hour_clock =
      Shell::Get()->system_tray_model()->clock()->hour_clock_type() ==
      base::k12HourClock;
  return use_12_hour_clock ? calendar_utils::GetTwelveHourClockTime(due)
                           : calendar_utils::GetTwentyFourHourClockTime(due);
}

std::unique_ptr<views::ImageView> BuildIcon() {
  return views::Builder<views::ImageView>()
      .SetBackground(views::CreateThemedRoundedRectBackground(
          cros_tokens::kCrosSysSystemOnBase1, kIconViewBackgroundRadius))
      .SetID(GlanceablesClassroomItemView::kIconViewId)
      // TODO(b/283370862): update icon.
      .SetImage(ui::ImageModel::FromVectorIcon(kUnifiedMenuLiveCaptionIcon,
                                               cros_tokens::kCrosSysOnSurface,
                                               kIconSize))
      .SetPreferredSize(kIconViewPreferredSize)
      .SetProperty(views::kMarginsKey, kIconViewMargin)
      .Build();
}

std::unique_ptr<views::BoxLayoutView> BuildAssignmentTitleLabels(
    const GlanceablesClassroomStudentAssignment* assignment) {
  const auto* const typography_provider = TypographyProvider::Get();

  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter)
      .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
      .SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded))
      .AddChild(
          views::Builder<views::Label>()
              .SetText(base::UTF8ToUTF16(assignment->course_work_title))
              .SetID(GlanceablesClassroomItemView::kCourseWorkTitleLabelId)
              .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
              .SetFontList(typography_provider->ResolveTypographyToken(
                  TypographyToken::kCrosButton2))
              .SetLineHeight(typography_provider->ResolveLineHeight(
                  TypographyToken::kCrosButton2)))
      .AddChild(views::Builder<views::Label>()
                    .SetText(base::UTF8ToUTF16(assignment->course_title))
                    .SetID(GlanceablesClassroomItemView::kCourseTitleLabelId)
                    .SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant)
                    .SetFontList(typography_provider->ResolveTypographyToken(
                        TypographyToken::kCrosAnnotation1))
                    .SetLineHeight(typography_provider->ResolveLineHeight(
                        TypographyToken::kCrosAnnotation1)))
      .Build();
}

std::unique_ptr<views::BoxLayoutView> BuildDueLabels(
    const GlanceablesClassroomStudentAssignment* assignment) {
  const auto* const typography_provider = TypographyProvider::Get();

  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter)
      .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kEnd)
      .SetProperty(views::kMarginsKey, kDueLabelsMargin)
      .AddChild(views::Builder<views::Label>()
                    .SetText(GetFormattedDueDate(assignment->due.value()))
                    .SetID(GlanceablesClassroomItemView::kDueDateLabelId)
                    .SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant)
                    .SetFontList(typography_provider->ResolveTypographyToken(
                        TypographyToken::kCrosAnnotation1))
                    .SetLineHeight(typography_provider->ResolveLineHeight(
                        TypographyToken::kCrosAnnotation1)))
      .AddChild(views::Builder<views::Label>()
                    .SetText(GetFormattedDueTime(assignment->due.value()))
                    .SetID(GlanceablesClassroomItemView::kDueTimeLabelId)
                    .SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant)
                    .SetFontList(typography_provider->ResolveTypographyToken(
                        TypographyToken::kCrosAnnotation1))
                    .SetLineHeight(typography_provider->ResolveLineHeight(
                        TypographyToken::kCrosAnnotation1)))
      .Build();
}

}  // namespace

GlanceablesClassroomItemView::GlanceablesClassroomItemView(
    const GlanceablesClassroomStudentAssignment* assignment) {
  CHECK(assignment);

  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBase, kBackgroundRadius));
  SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  SetInteriorMargin(kInteriorMargin);

  AddChildView(BuildIcon());
  AddChildView(BuildAssignmentTitleLabels(assignment));
  if (assignment->due.has_value()) {
    AddChildView(BuildDueLabels(assignment));
  }
}

GlanceablesClassroomItemView::~GlanceablesClassroomItemView() = default;

BEGIN_METADATA(GlanceablesClassroomItemView, views::View)
END_METADATA

}  // namespace ash
