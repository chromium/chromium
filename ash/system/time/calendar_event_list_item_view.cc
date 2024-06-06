// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_event_list_item_view.h"

#include <string>
#include <string_view>

#include "ash/bubble/bubble_utils.h"
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
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
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

// The paddings for `CalendarEventListItemView`.
constexpr auto kEventListItemInsets =
    gfx::Insets::VH(8, calendar_utils::kEventListItemViewStartEndMargin);
constexpr auto kEventListItemHorizontalChildSpacing = 8;
constexpr int kEventListItemCornerRadius = 16;
constexpr int kEventListItemCornerDefaultRadius = 5;
constexpr int kUpNextViewEventListItemCornerRadius = 20;
constexpr float kEventListItemFocusCornerRadius = 3.0f;

// Radius of the event color dot.
constexpr int kColorDotRadius = 4;

// Dimension of the event color dot view.
constexpr int kColorDotViewSize = kColorDotRadius * 2;

// Default Calendar API color ID to use when no event color is specifified.
constexpr char kDefaultColorId[] = "7";

// The color ID for past event items.
constexpr char kPastEventsColorId[] = "0";

// Map of Calendar API color ids and their respective hex color code. For color
// id "0", it's not part of the Calendar API color, but it's used for past
// events for a gray out effect.
constexpr auto kEventHexColorCodes =
    base::MakeFixedFlatMap<std::string_view, std::string_view>(
        {{"0", "1B1B1F"},
         {"1", "6994FF"},
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

// In Multi-Calendar, events without a custom color are injected with the color
// ID of the calendar they belong to in order to maintain a visible distinction
// between calendars when viewing events.
// Modified events have been prepended with a marker (`kInjectedColorIdPrefix`)
// to indicate that the calendar color map should be used to decode the color.
constexpr auto kCalendarHexColorCodes =
    base::MakeFixedFlatMap<std::string_view, std::string_view>(
        {{"c1", "ac725e"},  {"c2", "d06b64"},  {"c3", "f83a22"},
         {"c4", "fa573c"},  {"c5", "ff7537"},  {"c6", "ffad46"},
         {"c7", "42d692"},  {"c8", "16a765"},  {"c9", "7bd148"},
         {"c10", "b3dc6c"}, {"c11", "fbe983"}, {"c12", "fad165"},
         {"c13", "92e1c0"}, {"c14", "9fe1e7"}, {"c15", "9fc6e7"},
         {"c16", "4986e7"}, {"c17", "9a9cff"}, {"c18", "b99aff"},
         {"c19", "c2c2c2"}, {"c20", "cabdbf"}, {"c21", "cca6ac"},
         {"c22", "f691b2"}, {"c23", "cd74e6"}, {"c24", "a47ae2"}});

constexpr SkAlpha SK_Alpha38Opacity = 0x61;
constexpr SkAlpha SK_Alpha50Opacity = 0x80;

// Renders an Event color dot.
class CalendarEventListItemDot : public views::View {
  METADATA_HEADER(CalendarEventListItemDot, views::View)

 public:
  explicit CalendarEventListItemDot(std::string color_id)
      : alpha_(color_id == kPastEventsColorId ? SK_Alpha38Opacity
                                              : SK_AlphaOPAQUE) {
    CHECK(color_id.empty() || kEventHexColorCodes.count(color_id) ||
          kCalendarHexColorCodes.count(color_id));

    std::string_view hex_code = LookupColorId(color_id);
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
    color_dot.setColor(SkColorSetA(color_, alpha_));
    color_dot.setStyle(cc::PaintFlags::kFill_Style);
    color_dot.setAntiAlias(true);
    canvas->DrawCircle(GetContentsBounds().CenterPoint(), kColorDotRadius,
                       color_dot);
  }

 private:
  std::string_view LookupColorId(std::string color_id) {
    const auto event_color_iter = kEventHexColorCodes.find(color_id);
    if (event_color_iter != kEventHexColorCodes.end()) {
      return event_color_iter->second;
    }
    if (calendar_utils::IsMultiCalendarEnabled()) {
      const auto cal_color_iter = kCalendarHexColorCodes.find(color_id);
      if (cal_color_iter != kCalendarHexColorCodes.end()) {
        return cal_color_iter->second;
      }
    }
    return kEventHexColorCodes.at(kDefaultColorId);
  }

  // The color value and the opacity of the dot.
  int color_;
  const SkAlpha alpha_;
};

BEGIN_METADATA(CalendarEventListItemDot)
END_METADATA

// Creates and returns a label containing the event summary.
views::Builder<views::Label> CreateSummaryLabel(
    const std::string& event_summary,
    const std::u16string& tooltip_text,
    const int& fixed_width,
    const bool is_past_event) {
  return views::Builder<views::Label>(
             bubble_utils::CreateLabel(
                 TypographyToken::kCrosButton2,
                 event_summary.empty()
                     ? l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_NO_TITLE)
                     : base::UTF8ToUTF16(event_summary),
                 is_past_event ? cros_tokens::kCrosSysDisabled
                               : cros_tokens::kCrosSysOnSurface))
      .SetID(kSummaryLabelID)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetAutoColorReadabilityEnabled(false)
      .SetMultiLine(true)
      .SetMaxLines(1)
      .SizeToFit(fixed_width)
      .SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL)
      .SetSubpixelRenderingEnabled(false)
      .SetTooltipText(tooltip_text);
}

// Creates and returns a label containing the event time.
views::Builder<views::Label> CreateTimeLabel(const std::u16string& title,
                                             const std::u16string& tooltip_text,
                                             const bool is_past_event) {
  return views::Builder<views::Label>(
             bubble_utils::CreateLabel(
                 TypographyToken::kCrosAnnotation1, title,
                 is_past_event ? cros_tokens::kCrosSysDisabled
                               : cros_tokens::kCrosSysOnSurfaceVariant))
      .SetID(kTimeLabelID)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetAutoColorReadabilityEnabled(false)
      .SetElideBehavior(gfx::ElideBehavior::NO_ELIDE)
      .SetSubpixelRenderingEnabled(false)
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
  std::optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override {
    return gfx::RRectF(rect, corners_);
  }

 private:
  // The user-supplied rounded rect corners.
  const gfx::RoundedCornersF corners_;
};

}  // namespace

CalendarEventListItemView::CalendarEventListItemView(
    CalendarViewController* calendar_view_controller,
    SelectedDateParams selected_date_params,
    google_apis::calendar::CalendarEvent event,
    UIParams ui_params,
    EventListItemIndex event_list_item_index)
    : calendar_view_controller_(calendar_view_controller),
      selected_date_params_(selected_date_params),
      event_url_(event.html_link()),
      video_conference_url_(event.conference_data_uri()) {
  SetCallback(base::BindRepeating(&CalendarEventListItemView::PerformAction,
                                  base::Unretained(this)));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  const auto [start_time, end_time] = calendar_utils::GetStartAndEndTime(
      &event, selected_date_params_.selected_date,
      selected_date_params_.selected_date_midnight,
      selected_date_params_.selected_date_midnight_utc);
  const auto [start_time_accessible_name, end_time_accessible_name] =
      event_date_formatter_util::GetStartAndEndTimeAccessibleNames(start_time,
                                                                   end_time);
  GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  const std::u16string event_item_index_in_list_string =
      l10n_util::GetStringFUTF16(
          IDS_ASH_CALENDAR_EVENT_POSITION_ACCESSIBLE_DESCRIPTION,
          base::NumberToString16(event_list_item_index.item_index),
          base::NumberToString16(event_list_item_index.total_count_of_events));
  GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
      IDS_ASH_CALENDAR_EVENT_ENTRY_ACCESSIBLE_DESCRIPTION,
      event_item_index_in_list_string, base::UTF8ToUTF16(event.summary()),
      start_time_accessible_name, end_time_accessible_name,
      calendar_utils::GetTimeZone(start_time)));
  SetFocusBehavior(FocusBehavior::ALWAYS);

  // Conditionally round the items corners depending upon where it sits in the
  // list and whether it is in the up next event list.
  const int round_corner_radius = ui_params.is_up_next_event_list_item
                                      ? kUpNextViewEventListItemCornerRadius
                                      : kEventListItemCornerRadius;
  const int top_radius = ui_params.round_top_corners
                             ? round_corner_radius
                             : kEventListItemCornerDefaultRadius;
  const int bottom_radius = ui_params.round_bottom_corners
                                ? round_corner_radius
                                : kEventListItemCornerDefaultRadius;
  const gfx::RoundedCornersF item_corner_radius = gfx::RoundedCornersF(
      top_radius, top_radius, bottom_radius, bottom_radius);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(item_corner_radius);
  SetUpFocusHighlight(item_corner_radius);

  std::u16string formatted_time_text;

  is_past_event_ = end_time < base::Time::NowFromSystemTime();
  if (calendar_utils::IsMultiDayEvent(&event) || event.all_day_event()) {
    formatted_time_text = event_date_formatter_util::GetMultiDayText(
        &event, selected_date_params_.selected_date_midnight,
        selected_date_params_.selected_date_midnight_utc);
  } else {
    formatted_time_text =
        event_date_formatter_util::GetFormattedInterval(start_time, end_time);
    is_current_or_next_single_day_event_ = !is_past_event_;
  }

  const auto tooltip_text = l10n_util::GetStringFUTF16(
      IDS_ASH_CALENDAR_EVENT_ENTRY_TOOL_TIP, base::UTF8ToUTF16(event.summary()),
      formatted_time_text);

  auto horizontal_layout_manager = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kEventListItemInsets,
      kEventListItemHorizontalChildSpacing);
  horizontal_layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

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

    // TODO(crbug.com/40232718): See View::SetLayoutManagerUseConstrainedSpace.
    event_list_dot_container->SetLayoutManagerUseConstrainedSpace(false);

    event_list_dot_container
        ->AddChildView(std::make_unique<CalendarEventListItemDot>(
            is_past_event_ ? kPastEventsColorId : event.color_id()))
        ->SetID(kEventListItemDotID);
  }

  // Labels.
  views::View* vertical_container =
      horizontal_container->AddChildView(std::make_unique<views::View>());
  vertical_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  vertical_container->AddChildView(
      CreateSummaryLabel(event.summary(), tooltip_text, ui_params.fixed_width,
                         is_past_event_)
          .Build());
  vertical_container->AddChildView(
      CreateTimeLabel(formatted_time_text, tooltip_text, is_past_event_)
          .Build());
  horizontal_container_layout_manager->SetFlexForView(vertical_container, 1);
  // TODO(crbug.com/40232718): See View::SetLayoutManagerUseConstrainedSpace.
  // `vertical_container` has 1 flex. This causes the passed constraint space to
  // be fully occupied. Thus causing the view to become larger.
  horizontal_container->SetLayoutManagerUseConstrainedSpace(false);

  // Join button. Only shows it if the event is not the past event.
  if (!video_conference_url_.is_empty() && !is_past_event_) {
    auto join_button = std::make_unique<PillButton>(
        base::BindRepeating(
            &CalendarEventListItemView::OnJoinMeetingButtonPressed,
            weak_ptr_factory_.GetWeakPtr()),
        l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_JOIN_BUTTON),
        PillButton::Type::kDefaultWithoutIcon);
    join_button->GetViewAccessibility().SetName(
        l10n_util::GetStringFUTF16(IDS_ASH_CALENDAR_JOIN_BUTTON_ACCESSIBLE_NAME,
                                   base::UTF8ToUTF16(event.summary())));
    join_button->SetID(kJoinButtonID);
    horizontal_container->AddChildView(std::move(join_button));
  }
}

CalendarEventListItemView::~CalendarEventListItemView() = default;

void CalendarEventListItemView::OnThemeChanged() {
  views::View::OnThemeChanged();
  SetBackground(views::CreateSolidBackground(
      SkColorSetA(GetColorProvider()->GetColor(cros_tokens::kCrosSysSurface5),
                  is_past_event_ ? SK_Alpha50Opacity : SK_AlphaOPAQUE)));
}

void CalendarEventListItemView::PerformAction(const ui::Event& event) {
  DCHECK(event_url_.is_empty() || event_url_.is_valid());

  calendar_view_controller_->RecordEventListItemActivated(event);
  calendar_view_controller_->OnCalendarEventWillLaunch();

  GURL finalized_url;
  bool opened_pwa = false;
  Shell::Get()->system_tray_model()->client()->ShowCalendarEvent(
      event_url_, selected_date_params_.selected_date_midnight, opened_pwa,
      finalized_url);
}

void CalendarEventListItemView::SetUpFocusHighlight(
    const gfx::RoundedCornersF& item_corner_radius) {
  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/false,
                                   /*background_color=*/
                                   gfx::kPlaceholderColor);
  views::FocusRing::Get(this)->SetColorId(cros_tokens::kCrosSysFocusRing);
  views::FocusRing::Get(this)->SetHaloThickness(
      kEventListItemFocusCornerRadius);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<RoundedCornerHighlightPathGenerator>(
                item_corner_radius));
  // Unset the focus painter set by `HoverHighlightView`.
  SetFocusPainter(nullptr);
}

void CalendarEventListItemView::OnJoinMeetingButtonPressed(
    const ui::Event& event) {
  calendar_view_controller_->RecordJoinMeetingButtonPressed(event);

  // The join button won't be shown if `video_conference_url_` doesn't have a
  // value.
  DCHECK(!video_conference_url_.is_empty());
  Shell::Get()->system_tray_model()->client()->ShowVideoConference(
      video_conference_url_);
}

BEGIN_METADATA(CalendarEventListItemView);
END_METADATA

}  // namespace ash
