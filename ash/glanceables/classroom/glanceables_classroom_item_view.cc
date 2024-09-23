// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/classroom/glanceables_classroom_item_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/date_helper.h"
#include "base/check.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

// Styles for the whole `GlanceablesClassroomItemView`.
constexpr int kFocusRingCornerRadius = 14;
constexpr auto kClassroomItemMargin = gfx::Insets::VH(4, 0);

// Styles for the icon view.
constexpr int kIconViewBackgroundRadius = 12;
constexpr auto kIconViewMargin = gfx::Insets::TLBR(0, 0, 0, 12);
constexpr auto kIconViewPreferredSize =
    gfx::Size(kIconViewBackgroundRadius * 2, kIconViewBackgroundRadius * 2);
constexpr int kIconSize = 16;

// Styles for the assignment labels.
constexpr int kAssignmentBetweenLabelsSpacing = 2;
constexpr auto kAssignmentLabelsMargin = gfx::Insets::TLBR(2, 0, 0, 16);
constexpr auto kAssignmentCourseWorkTypography = TypographyToken::kCrosButton2;
constexpr auto kAssignmentCourseTypography = TypographyToken::kCrosAnnotation1;

// Styles for the container containing due date and time labels.
constexpr auto kDueLabelsMargin = gfx::Insets::TLBR(2, 0, 0, 4);
constexpr auto kDueLabelsTypography = TypographyToken::kCrosAnnotation1;

constexpr char kDayOfWeekFormatterPattern[] = "EEE";      // "Wed"
constexpr char kMonthAndDayFormatterPattern[] = "MMM d";  // "Feb 28"

std::u16string GetFormattedDueDate(const base::Time& due) {
  const auto midnight_today = base::Time::Now().LocalMidnight();
  const auto midnight_tomorrow = midnight_today + base::Days(1);

  if (midnight_today <= due && due < midnight_tomorrow) {
    return l10n_util::GetStringUTF16(IDS_GLANCEABLES_DUE_TODAY);
  }

  const auto midnight_in_7_days_from_today = midnight_today + base::Days(7);
  auto* const date_helper = DateHelper::GetInstance();
  CHECK(date_helper);

  const auto formatter = date_helper->CreateSimpleDateFormatter(
      midnight_tomorrow <= due && due < midnight_in_7_days_from_today
          ? kDayOfWeekFormatterPattern
          : kMonthAndDayFormatterPattern);
  return date_helper->GetFormattedTime(&formatter, due);
}

std::u16string GetFormattedDueTime(const base::Time& due) {
  base::Time::Exploded exploded_due;
  due.LocalExplode(&exploded_due);
  if (exploded_due.hour == 23 && exploded_due.minute == 59) {
    // Do not render time for assignments without specified due time (in this
    // case API automatically sets due time to 23:59).
    // NOTE: there is no way to differentiate missing due time vs. explicitly
    // set to 23:59 by user. Though the second case is less likely and
    // rendering it do not bring much value.
    return std::u16string();
  }

  const bool use_12_hour_clock =
      Shell::Get()->system_tray_model()->clock()->hour_clock_type() ==
      base::k12HourClock;
  return use_12_hour_clock ? calendar_utils::GetTwelveHourClockTime(due)
                           : calendar_utils::GetTwentyFourHourClockTime(due);
}

std::unique_ptr<views::View> BuildIcon() {
  return views::Builder<views::ImageView>()
      .SetBackground(views::CreateThemedRoundedRectBackground(
          cros_tokens::kCrosSysSystemOnBase1, kIconViewBackgroundRadius))
      .SetID(base::to_underlying(GlanceablesViewId::kClassroomItemIcon))
      .SetImage(ui::ImageModel::FromVectorIcon(
          kGlanceablesClassroomAssignmentIcon, cros_tokens::kCrosSysOnSurface,
          kIconSize))
      .SetPreferredSize(kIconViewPreferredSize)
      .SetProperty(views::kMarginsKey, kIconViewMargin)
      .Build();
}

std::unique_ptr<views::BoxLayoutView> BuildAssignmentTitleLabels(
    const GlanceablesClassroomAssignment* assignment) {
  const auto* const typography_provider = TypographyProvider::Get();

  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter)
      .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
      .SetBetweenChildSpacing(kAssignmentBetweenLabelsSpacing)
      .SetProperty(views::kMarginsKey, kAssignmentLabelsMargin)
      .SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::BoxLayout::Orientation::kHorizontal,
                                   views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded))
      .AddChild(views::Builder<views::Label>()
                    .SetText(base::UTF8ToUTF16(assignment->course_work_title))
                    .SetID(base::to_underlying(
                        GlanceablesViewId::kClassroomItemCourseWorkTitleLabel))
                    .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
                    .SetFontList(typography_provider->ResolveTypographyToken(
                        kAssignmentCourseWorkTypography))
                    .SetLineHeight(typography_provider->ResolveLineHeight(
                        kAssignmentCourseWorkTypography)))
      .AddChild(views::Builder<views::Label>()
                    .SetText(base::UTF8ToUTF16(assignment->course_title))
                    .SetID(base::to_underlying(
                        GlanceablesViewId::kClassroomItemCourseTitleLabel))
                    .SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant)
                    .SetFontList(typography_provider->ResolveTypographyToken(
                        kAssignmentCourseTypography))
                    .SetLineHeight(typography_provider->ResolveLineHeight(
                        kAssignmentCourseTypography)))
      .Build();
}

std::unique_ptr<views::BoxLayoutView> BuildDueLabels(
    const std::u16string& due_date,
    const std::u16string& due_time) {
  const auto* const typography_provider = TypographyProvider::Get();

  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter)
      .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kEnd)
      .SetBetweenChildSpacing(kAssignmentBetweenLabelsSpacing)
      .SetProperty(views::kMarginsKey, kDueLabelsMargin)
      .AddChild(views::Builder<views::Label>()
                    .SetText(due_date)
                    .SetID(base::to_underlying(
                        GlanceablesViewId::kClassroomItemDueDateLabel))
                    .SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant)
                    .SetFontList(typography_provider->ResolveTypographyToken(
                        kDueLabelsTypography))
                    // Use the course work line height to align with the
                    // assignment labels.
                    .SetLineHeight(typography_provider->ResolveLineHeight(
                        kAssignmentCourseWorkTypography)))
      .AddChild(views::Builder<views::Label>()
                    .SetText(due_time)
                    .SetID(base::to_underlying(
                        GlanceablesViewId::kClassroomItemDueTimeLabel))
                    .SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant)
                    .SetFontList(typography_provider->ResolveTypographyToken(
                        kDueLabelsTypography))
                    .SetLineHeight(typography_provider->ResolveLineHeight(
                        kDueLabelsTypography)))
      .Build();
}

}  // namespace

GlanceablesClassroomItemView::GlanceablesClassroomItemView(
    const GlanceablesClassroomAssignment* assignment,
    base::RepeatingClosure pressed_callback)
    : views::Button(std::move(pressed_callback)) {
  CHECK(assignment);

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  SetProperty(views::kMarginsKey, kClassroomItemMargin);
  std::vector<std::u16string> a11y_description_parts{
      base::UTF8ToUTF16(assignment->course_title)};

  AddChildView(BuildIcon());
  AddChildView(BuildAssignmentTitleLabels(assignment));
  if (assignment->due.has_value()) {
    const auto due_date = GetFormattedDueDate(assignment->due.value());
    const auto due_time = GetFormattedDueTime(assignment->due.value());
    AddChildView(BuildDueLabels(due_date, due_time));

    a11y_description_parts.push_back(l10n_util::GetStringFUTF16(
        IDS_GLANCEABLES_CLASSROOM_ASSIGNMENT_DUE_ACCESSIBLE_DESCRIPTION,
        due_time.empty() ? due_date
                         : base::JoinString({due_date, due_time}, u", ")));
  }

  GetViewAccessibility().SetRole(ax::mojom::Role::kListItem);
  GetViewAccessibility().SetIsLeaf(true);
  GetViewAccessibility().SetName(
      base::UTF8ToUTF16(assignment->course_work_title));
  GetViewAccessibility().SetDescription(
      base::JoinString(a11y_description_parts, u", "));
  UpdateAccessibleDefaultAction();

  views::FocusRing::Install(this);
  views::FocusRing* const focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColorId(cros_tokens::kCrosSysFocusRing);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<views::RoundRectHighlightPathGenerator>(
                gfx::Insets(), gfx::RoundedCornersF(kFocusRingCornerRadius)));

  // Prevent the layout manager from setting the focus ring to a default hidden
  // visibility.
  focus_ring->SetProperty(views::kViewIgnoredByLayoutKey, true);
}

GlanceablesClassroomItemView::~GlanceablesClassroomItemView() = default;

void GlanceablesClassroomItemView::Layout(PassKey) {
  LayoutSuperclass<views::Button>(this);
  views::FocusRing::Get(this)->DeprecatedLayoutImmediately();
}

void GlanceablesClassroomItemView::OnEnabledChanged() {
  views::Button::OnEnabledChanged();
  UpdateAccessibleDefaultAction();
}

void GlanceablesClassroomItemView::UpdateAccessibleDefaultAction() {
  GetViewAccessibility().SetDefaultActionVerb(
      ax::mojom::DefaultActionVerb::kClick);
}

BEGIN_METADATA(GlanceablesClassroomItemView)
END_METADATA

}  // namespace ash
