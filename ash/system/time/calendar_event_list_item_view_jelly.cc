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
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_metrics.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/system/time/event_date_formatter_util.h"
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
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {

// The paddings for `CalendarEventListViewItemJelly`.
constexpr auto kEventListItemDotOffset = 12;
constexpr auto kEventListItemInsets =
    gfx::Insets::TLBR(6, kEventListItemDotOffset, 6, 12);
constexpr auto kEventListItemHorizontalChildSpacing = 8;

// Radius of the event color dot.
constexpr int kColorDotRadius = 4;

// Dimension of the event color dot view.
constexpr int kColorDotViewSize = 8;

// Default Calendar API color ID to use when no event color is specifified.
constexpr char kDefaultColorId[] = "7";

// Map of Calendar API color ids and their respective hex color code.
std::map<std::string, std::string> kEventHexColorCodes = {
    {"1", "a4bdfc"}, {"2", "7ae7bf"},  {"3", "dbadff"}, {"4", "ff887c"},
    {"5", "fbd75b"}, {"6", "ffb878"},  {"7", "46d6db"}, {"8", "e1e1e1"},
    {"9", "5484ed"}, {"10", "51b749"}, {"11", "dc2127"}};

// Renders an Event color dot.
class CalendarEventListItemDot : public views::View {
 public:
  explicit CalendarEventListItemDot(std::string color_id) {
    DCHECK(color_id.empty() || kEventHexColorCodes.count(color_id));
    std::string hex_code =
        kEventHexColorCodes[color_id.empty() ? kDefaultColorId : color_id];
    base::HexStringToInt(hex_code, &color_);
    SetPreferredSize(gfx::Size(kColorDotViewSize,
                               kColorDotViewSize + kEventListItemDotOffset));
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
  // The color value of the dot.
  int color_;
};

// Creates and returns a label containing the event summary.
views::Builder<views::Label> CreateSummaryLabel(
    const std::string& event_summary,
    const std::u16string& tooltip_text,
    const int& max_width) {
  return views::Builder<views::Label>(
             bubble_utils::CreateLabel(
                 bubble_utils::TypographyStyle::kButton1,
                 event_summary.empty()
                     ? l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_NO_TITLE)
                     : base::UTF8ToUTF16(event_summary)))
      .SetID(kSummaryLabelID)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetAutoColorReadabilityEnabled(false)
      .SetMultiLine(true)
      .SetMaxLines(1)
      .SetMaximumWidth(max_width)
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
             bubble_utils::CreateLabel(bubble_utils::TypographyStyle::kBody1,
                                       title))
      .SetID(kTimeLabelID)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetAutoColorReadabilityEnabled(false)
      .SetElideBehavior(gfx::ElideBehavior::NO_ELIDE)
      .SetSubpixelRenderingEnabled(false)
      .SetTextContext(CONTEXT_CALENDAR_DATE)
      .SetTooltipText(tooltip_text);
}

}  // namespace

CalendarEventListItemViewJelly::CalendarEventListItemViewJelly(
    CalendarViewController* calendar_view_controller,
    SelectedDateParams selected_date_params,
    google_apis::calendar::CalendarEvent event,
    const bool round_top_corners,
    const bool round_bottom_corners,
    const int max_width)
    : ActionableView(TrayPopupInkDropStyle::FILL_BOUNDS),
      calendar_view_controller_(calendar_view_controller),
      selected_date_params_(selected_date_params),
      event_url_(event.html_link()) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  const auto [start_time, end_time] = calendar_utils::GetStartAndEndTime(
      &event, selected_date_params_.selected_date,
      selected_date_params_.selected_date_midnight,
      selected_date_params_.selected_date_midnight_utc);
  const auto [start_time_accessible_name, end_time_accessible_name] =
      event_date_formatter_util::GetStartAndEndTimeAccessibleNames(start_time,
                                                                   end_time);
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kButton);
  SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_ASH_CALENDAR_EVENT_ENTRY_ACCESSIBLE_DESCRIPTION,
      start_time_accessible_name, end_time_accessible_name,
      calendar_utils::GetTimeZone(start_time),
      base::UTF8ToUTF16(event.summary())));
  SetFocusBehavior(FocusBehavior::ALWAYS);

  // Conditionally round the items corners depending upon where it sits in the
  // list.
  const int top_radius = round_top_corners ? 12 : 0;
  const int bottom_radius = round_bottom_corners ? 12 : 0;
  const gfx::RoundedCornersF item_corner_radius = gfx::RoundedCornersF(
      top_radius, top_radius, bottom_radius, bottom_radius);
  SetPaintToLayer();
  layer()->SetRoundedCornerRadius(item_corner_radius);

  auto horizontal_layout_manager = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kEventListItemInsets,
      kEventListItemHorizontalChildSpacing);
  horizontal_layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

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

  AddChildView(
      views::Builder<views::View>()
          .SetLayoutManager(std::move(horizontal_layout_manager))
          .AddChild(views::Builder<views::View>(
              std::make_unique<CalendarEventListItemDot>(event.color_id())))
          .AddChild(
              views::Builder<views::View>()
                  .SetLayoutManager(std::make_unique<views::BoxLayout>(
                      views::BoxLayout::Orientation::kVertical))
                  .AddChild(CreateSummaryLabel(event.summary(), tooltip_text,
                                               max_width))
                  .AddChild(CreateTimeLabel(formatted_time_text, tooltip_text)))
          .Build());
}

CalendarEventListItemViewJelly::~CalendarEventListItemViewJelly() = default;

void CalendarEventListItemViewJelly::OnThemeChanged() {
  views::View::OnThemeChanged();
  SetBackground(views::CreateSolidBackground(
      GetColorProvider()->GetColor(cros_tokens::kCrosSysSurface)));
}

bool CalendarEventListItemViewJelly::PerformAction(const ui::Event& event) {
  DCHECK(event_url_.is_empty() || event_url_.is_valid());

  calendar_metrics::RecordEventListItemActivated(event);
  calendar_view_controller_->OnCalendarEventWillLaunch();

  GURL finalized_url;
  bool opened_pwa = false;
  Shell::Get()->system_tray_model()->client()->ShowCalendarEvent(
      event_url_, selected_date_params_.selected_date_midnight, opened_pwa,
      finalized_url);
  return true;
}

BEGIN_METADATA(CalendarEventListItemViewJelly, views::View);
END_METADATA

}  // namespace ash
