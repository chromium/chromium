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
constexpr int kBackgroundRadius = 4;
constexpr int kLargeBackgroundRadius = 16;
constexpr auto kInteriorMargin = gfx::Insets::VH(8, 0);

// Styles for the icon view.
constexpr int kIconViewBackgroundRadius = 16;
constexpr auto kIconViewMargin = gfx::Insets::VH(4, 12);
constexpr auto kIconViewPreferredSize =
    gfx::Size(kIconViewBackgroundRadius * 2, kIconViewBackgroundRadius * 2);
constexpr int kIconSize = 20;

// Styles for the container containing due date and time labels.
constexpr auto kDueLabelsMargin = gfx::Insets::VH(0, 16);

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

std::u16string GetTurnedInAndGradedLabel(
    const GlanceablesClassroomAggregatedSubmissionsState& state) {
  return l10n_util::GetStringFUTF16(
      IDS_GLANCEABLES_ITEMS_TURNED_IN_AND_GRADED,
      base::NumberToString16(state.number_turned_in),
      base::NumberToString16(state.total_count),
      base::NumberToString16(state.number_graded));
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

  auto title_label_views =
      views::Builder<views::BoxLayoutView>()
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
                  .SetID(base::to_underlying(
                      GlanceablesViewId::kClassroomItemCourseWorkTitleLabel))
                  .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
                  .SetFontList(typography_provider->ResolveTypographyToken(
                      TypographyToken::kCrosButton2))
                  .SetLineHeight(typography_provider->ResolveLineHeight(
                      TypographyToken::kCrosButton2)))
          .AddChild(
              views::Builder<views::Label>()
                  .SetText(base::UTF8ToUTF16(assignment->course_title))
                  .SetID(base::to_underlying(
                      GlanceablesViewId::kClassroomItemCourseTitleLabel))
                  .SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant)
                  .SetFontList(typography_provider->ResolveTypographyToken(
                      TypographyToken::kCrosAnnotation1))
                  .SetLineHeight(typography_provider->ResolveLineHeight(
                      TypographyToken::kCrosAnnotation1)))
          .Build();
  if (assignment->submissions_state.has_value()) {
    title_label_views->AddChildView(
        views::Builder<views::Label>()
            .SetText(GetTurnedInAndGradedLabel(
                assignment->submissions_state.value()))
            .SetID(base::to_underlying(
                GlanceablesViewId::kClassroomItemTurnedInAndGradedLabel))
            .SetEnabledColorId(cros_tokens::kCrosSysPrimary)
            .SetFontList(typography_provider->ResolveTypographyToken(
                TypographyToken::kCrosBody2))
            .SetLineHeight(typography_provider->ResolveLineHeight(
                TypographyToken::kCrosBody2))
            .SetProperty(views::kMarginsKey, gfx::Insets::TLBR(4, 0, 0, 0))
            .Build());
  }
  return title_label_views;
}

std::unique_ptr<views::BoxLayoutView> BuildDueLabels(
    const std::u16string& due_date,
    const std::u16string& due_time) {
  const auto* const typography_provider = TypographyProvider::Get();

  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter)
      .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kEnd)
      .SetProperty(views::kMarginsKey, kDueLabelsMargin)
      .AddChild(views::Builder<views::Label>()
                    .SetText(due_date)
                    .SetID(base::to_underlying(
                        GlanceablesViewId::kClassroomItemDueDateLabel))
                    .SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant)
                    .SetFontList(typography_provider->ResolveTypographyToken(
                        TypographyToken::kCrosAnnotation1))
                    .SetLineHeight(typography_provider->ResolveLineHeight(
                        TypographyToken::kCrosAnnotation1)))
      .AddChild(views::Builder<views::Label>()
                    .SetText(due_time)
                    .SetID(base::to_underlying(
                        GlanceablesViewId::kClassroomItemDueTimeLabel))
                    .SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant)
                    .SetFontList(typography_provider->ResolveTypographyToken(
                        TypographyToken::kCrosAnnotation1))
                    .SetLineHeight(typography_provider->ResolveLineHeight(
                        TypographyToken::kCrosAnnotation1)))
      .Build();
}

// Returns rounded corners used for the item view. A larger radius is used for
// top corners when the item is the first in the list. A larger radius is used
// for bottom corners when the item is last in the list.
gfx::RoundedCornersF GetRoundedCorners(size_t index, size_t last_index) {
  size_t top_radius = (index == 0) ? kLargeBackgroundRadius : kBackgroundRadius;
  size_t bottom_radius =
      (index == last_index) ? kLargeBackgroundRadius : kBackgroundRadius;
  return gfx::RoundedCornersF(top_radius, top_radius, bottom_radius,
                              bottom_radius);
}

}  // namespace

GlanceablesClassroomItemView::GlanceablesClassroomItemView(
    const GlanceablesClassroomAssignment* assignment,
    base::RepeatingClosure pressed_callback,
    size_t item_index,
    size_t last_item_index)
    : views::Button(std::move(pressed_callback)) {
  CHECK(assignment);

  auto* const layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  layout->SetInteriorMargin(kInteriorMargin);

  const gfx::RoundedCornersF corner_radii =
      GetRoundedCorners(item_index, last_item_index);
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBase, corner_radii,
      /*for_border_thickness=*/0));

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

  if (assignment->submissions_state.has_value()) {
    a11y_description_parts.push_back(l10n_util::GetStringFUTF16(
        IDS_GLANCEABLES_CLASSROOM_ASSIGNMENT_SUBMISSIONS_STATE_ACCESSIBLE_DESCRIPTION,
        base::NumberToString16(assignment->submissions_state->number_turned_in),
        base::NumberToString16(assignment->submissions_state->total_count),
        base::NumberToString16(assignment->submissions_state->number_graded)));
  }

  SetAccessibleRole(ax::mojom::Role::kListItem);
  GetViewAccessibility().OverrideIsLeaf(true);
  SetAccessibleName(base::UTF8ToUTF16(assignment->course_work_title));
  SetAccessibleDescription(base::JoinString(a11y_description_parts, u", "));

  views::FocusRing::Install(this);
  views::FocusRing* const focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColorId(cros_tokens::kCrosSysFocusRing);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<views::RoundRectHighlightPathGenerator>(
                gfx::Insets(), corner_radii));

  // Prevent the layout manager from setting the focus ring to a default hidden
  // visibility.
  layout->SetChildViewIgnoredByLayout(focus_ring, true);
}

GlanceablesClassroomItemView::~GlanceablesClassroomItemView() = default;

void GlanceablesClassroomItemView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  views::Button::GetAccessibleNodeData(node_data);

  node_data->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kClick);
}

void GlanceablesClassroomItemView::Layout() {
  views::Button::Layout();
  views::FocusRing::Get(this)->Layout();
}

BEGIN_METADATA(GlanceablesClassroomItemView, views::View)
END_METADATA

}  // namespace ash
