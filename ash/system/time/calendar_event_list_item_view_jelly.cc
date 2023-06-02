// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_event_list_item_view_jelly.h"

#include <string>
#include <tuple>

#include "ash/bubble/bubble_utils.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/system/time/event_date_formatter_util.h"
#include "base/containers/fixed_flat_map.h"
#include "base/strings/utf_string_conversions.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {

// The paddings for `CalendarEventListViewItemJelly`.
constexpr auto kEventListItemInsets =
    gfx::Insets::VH(8, calendar_utils::kEventListItemViewStartEndMargin);
constexpr auto kEventListItemHorizontalChildSpacing = 8;
constexpr int kEventListItemCornerRadius = 16;
constexpr int kEventListItemCornerDefaultRadius = 4;
constexpr float kEventListItemFocusCornerRadius = 3.0f;

// Radius of the event color dot.
constexpr int kColorDotRadius = 4;

// Dimension of the event color dot view.
constexpr int kColorDotViewSize = kColorDotRadius * 2;

// Default Calendar API color ID to use when no event color is specifified.
constexpr char kDefaultColorId[] = "7";

// Map of Calendar API color ids and their respective hex color code.
constexpr auto kEventHexColorCodes =
    base::MakeFixedFlatMap<base::StringPiece, base::StringPiece>(
        {{"1", "6994FF"},
         {"2", "3CBD8E"},
         {"3", "BB74F2"},
         {"4", "FA827A"},
         {"5", "C7C419"},
         {"6", "F0A85F"},
         {"7", "60BDBD"},
         {"8", "BD9C9C"},
         {"9", "7077DC"},
         {"10", "5B9157"},
         {"11", "D45D5D"}});

// Renders an Event color dot.
class CalendarEventListItemDot : public views::View {
 public:
  explicit CalendarEventListItemDot(std::string color_id) {
    DCHECK(color_id.empty() || kEventHexColorCodes.count(color_id));

    base::BasicStringPiece<char> hex_code = LookupColorId(color_id);
    base::HexStringToInt(hex_code, &color_);
    SetPreferredSize(gfx::Size(
        kColorDotViewSize,
        kColorDotViewSize + calendar_utils::kEventListItemViewStartEndMargin));
  }
  CalendarEventListItemDot(const CalendarEventListItemDot& other) = delete;
  CalendarEventListItemDot& operator=(const CalendarEventListItemDot& other) =
      delete;
  ~CalendarEventListItemDot() override = default;

  // Draws the circle for the event color dot.
  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags color_dot;
    color_dot.setColor(SkColorSetA(color_, SK_AlphaOPAQUE));
    color_dot.setStyle(cc::PaintFlags::kFill_Style);
    color_dot.setAntiAlias(true);
    canvas->DrawCircle(GetContentsBounds().CenterPoint(), kColorDotRadius,
                       color_dot);
  }

 private:
  base::BasicStringPiece<char> LookupColorId(std::string color_id) {
    const auto* iter = kEventHexColorCodes.find(color_id);
    if (iter == kEventHexColorCodes.end()) {
      return kEventHexColorCodes.at(kDefaultColorId);
    }
    return iter->second;
  }

  // The color value of the dot.
  int color_;
};

// Creates and returns a label containing the event summary.
views::Builder<views::Label> CreateSummaryLabel(
    const std::string& event_summary,
    const std::u16string& tooltip_text,
    const int& fixed_width) {
  return views::Builder<views::Label>(
             bubble_utils::CreateLabel(
                 TypographyToken::kCrosButton2,
                 event_summary.empty()
                     ? l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_NO_TITLE)
                     : base::UTF8ToUTF16(event_summary),
                 cros_tokens::kCrosSysOnSurface))
      .SetID(kSummaryLabelID)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetAutoColorReadabilityEnabled(false)
      .SetMultiLine(true)
      .SetMaxLines(1)
      .SizeToFit(fixed_width)
      .SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL)
      .SetSubpixelRenderingEnabled(false)
      .SetTextContext(CONTEXT_CALENDAR_DATE)
      .SetTooltipText(tooltip_text);
}

// Creates and returns a label containing the event time.
views::Builder<views::Label> CreateTimeLabel(
    const std::u16string& title,
    const std::u16string& tooltip_text) {
  return views::Builder<views::Label>(
             bubble_utils::CreateLabel(TypographyToken::kCrosAnnotation1, title,
                                       cros_tokens::kCrosSysOnSurfaceVariant))
      .SetID(kTimeLabelID)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetAutoColorReadabilityEnabled(false)
      .SetElideBehavior(gfx::ElideBehavior::NO_ELIDE)
      .SetSubpixelRenderingEnabled(false)
      .SetTextContext(CONTEXT_CALENDAR_DATE)
      .SetTooltipText(tooltip_text);
}

// A `HighlightPathGenerator` that uses caller-supplied rounded rect corners.
class VIEWS_EXPORT RoundedCornerHighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  explicit RoundedCornerHighlightPathGenerator(
      const gfx::RoundedCornersF& corners)
      : corners_(corners) {}

  RoundedCornerHighlightPathGenerator(
      const RoundedCornerHighlightPathGenerator&) = delete;
  RoundedCornerHighlightPathGenerator& operator=(
      const RoundedCornerHighlightPathGenerator&) = delete;

  // views::HighlightPathGenerator:
  absl::optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override {
    return gfx::RRectF(rect, corners_);
  }

 private:
  // The user-supplied rounded rect corners.
  const gfx::RoundedCornersF corners_;
};

}  // namespace

CalendarEventListItemViewJelly::CalendarEventListItemViewJelly(
    CalendarViewController* calendar_view_controller,
    SelectedDateParams selected_date_params,
    google_apis::calendar::CalendarEvent event,
    UIParams ui_params,
    EventListItemIndex event_list_item_index)
    : ActionableView(TrayPopupInkDropStyle::FILL_BOUNDS),
      calendar_view_controller_(calendar_view_controller),
      selected_date_params_(selected_date_params),
      event_url_(event.html_link()),
      video_conference_url_(event.conference_data_uri()) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  const auto [start_time, end_time] = calendar_utils::GetStartAndEndTime(
      &event, selected_date_params_.selected_date,
      selected_date_params_.selected_date_midnight,
      selected_date_params_.selected_date_midnight_utc);
  const auto [start_time_accessible_name, end_time_accessible_name] =
      event_date_formatter_util::GetStartAndEndTimeAccessibleNames(start_time,
                                                                   end_time);
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kButton);
  const std::u16string event_item_index_in_list_string =
      l10n_util::GetStringFUTF16(
          IDS_ASH_CALENDAR_EVENT_POSITION_ACCESSIBLE_DESCRIPTION,
          base::NumberToString16(event_list_item_index.item_index),
          base::NumberToString16(event_list_item_index.total_count_of_events));
  SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_ASH_CALENDAR_EVENT_ENTRY_ACCESSIBLE_DESCRIPTION_JELLY,
      event_item_index_in_list_string, base::UTF8ToUTF16(event.summary()),
      start_time_accessible_name, end_time_accessible_name,
      calendar_utils::GetTimeZone(start_time)));
  SetFocusBehavior(FocusBehavior::ALWAYS);

  // Conditionally round the items corners depending upon where it sits in the
  // list.
  const int top_radius = ui_params.round_top_corners
                             ? kEventListItemCornerRadius
                             : kEventListItemCornerDefaultRadius;
  const int bottom_radius = ui_params.round_bottom_corners
                                ? kEventListItemCornerRadius
                                : kEventListItemCornerDefaultRadius;
  const gfx::RoundedCornersF item_corner_radius = gfx::RoundedCornersF(
      top_radius, top_radius, bottom_radius, bottom_radius);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(item_corner_radius);
  SetUpFocusHighlight(item_corner_radius);

  std::u16string formatted_time_text;
  if (calendar_utils::IsMultiDayEvent(&event) || event.all_day_event()) {
    formatted_time_text = event_date_formatter_util::GetMultiDayText(
        &event, selected_date_params_.selected_date_midnight,
        selected_date_params_.selected_date_midnight_utc);
  } else {
    formatted_time_text =
        event_date_formatter_util::GetFormattedInterval(start_time, end_time);
  }
  const auto tooltip_text = l10n_util::GetStringFUTF16(
      IDS_ASH_CALENDAR_EVENT_ENTRY_TOOL_TIP, base::UTF8ToUTF16(event.summary()),
      formatted_time_text);

  auto horizontal_layout_manager = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kEventListItemInsets,
      kEventListItemHorizontalChildSpacing);
  views::View* horizontal_container =
      AddChildView(std::make_unique<views::View>());
  auto* horizontal_container_layout_manager =
      horizontal_container->SetLayoutManager(
          std::move(horizontal_layout_manager));

  // Event list dot.
  if (ui_params.show_event_list_dot) {
    views::View* event_list_dot_container =
        horizontal_container->AddChildView(std::make_unique<views::View>());
    auto* layout_vertical_start = event_list_dot_container->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
    layout_vertical_start->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kStart);
    event_list_dot_container
        ->AddChildView(
            std::make_unique<CalendarEventListItemDot>(event.color_id()))
        ->SetID(kEventListItemDotID);
  }

  // Labels.
  views::View* vertical_container =
      horizontal_container->AddChildView(std::make_unique<views::View>());
  vertical_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  vertical_container->AddChildView(
      CreateSummaryLabel(event.summary(), tooltip_text, ui_params.fixed_width)
          .Build());
  vertical_container->AddChildView(
      CreateTimeLabel(formatted_time_text, tooltip_text).Build());
  horizontal_container_layout_manager->SetFlexForView(vertical_container, 1);

  // Join button.
  if (!video_conference_url_.is_empty()) {
    views::View* join_button_container =
        horizontal_container->AddChildView(std::make_unique<views::View>());
    auto* layout_vertical_center = join_button_container->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
    layout_vertical_center->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    auto join_button = std::make_unique<PillButton>(
        base::BindRepeating(
            &CalendarEventListItemViewJelly::OnJoinMeetingButtonPressed,
            weak_ptr_factory_.GetWeakPtr()),
        l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_JOIN_BUTTON),
        PillButton::Type::kDefaultWithoutIcon);
    join_button->SetAccessibleName(
        l10n_util::GetStringFUTF16(IDS_ASH_CALENDAR_JOIN_BUTTON_ACCESSIBLE_NAME,
                                   base::UTF8ToUTF16(event.summary())));
    join_button->SetID(kJoinButtonID);
    join_button_container->AddChildView(std::move(join_button));
  }
}

CalendarEventListItemViewJelly::~CalendarEventListItemViewJelly() = default;

void CalendarEventListItemViewJelly::OnThemeChanged() {
  views::View::OnThemeChanged();
  SetBackground(views::CreateSolidBackground(
      GetColorProvider()->GetColor(cros_tokens::kCrosSysSurface5)));
}

bool CalendarEventListItemViewJelly::PerformAction(const ui::Event& event) {
  DCHECK(event_url_.is_empty() || event_url_.is_valid());

  calendar_view_controller_->RecordEventListItemActivated(event);
  calendar_view_controller_->OnCalendarEventWillLaunch();

  GURL finalized_url;
  bool opened_pwa = false;
  Shell::Get()->system_tray_model()->client()->ShowCalendarEvent(
      event_url_, selected_date_params_.selected_date_midnight, opened_pwa,
      finalized_url);
  return true;
}

void CalendarEventListItemViewJelly::SetUpFocusHighlight(
    const gfx::RoundedCornersF& item_corner_radius) {
  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/false,
                                   /*background_color=*/
                                   gfx::kPlaceholderColor);
  views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);
  views::FocusRing::Get(this)->SetHaloThickness(
      kEventListItemFocusCornerRadius);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<RoundedCornerHighlightPathGenerator>(
                item_corner_radius));
  // Unset the focus painter set by `ActionableView`.
  SetFocusPainter(nullptr);
}

void CalendarEventListItemViewJelly::OnJoinMeetingButtonPressed(
    const ui::Event& event) {
  calendar_view_controller_->RecordJoinMeetingButtonPressed(event);

  // The join button won't be shown if `video_conference_url_` doesn't have a
  // value.
  DCHECK(!video_conference_url_.is_empty());
  Shell::Get()->system_tray_model()->client()->ShowVideoConference(
      video_conference_url_);
}

BEGIN_METADATA(CalendarEventListItemViewJelly, views::View);
END_METADATA

}  // namespace ash
