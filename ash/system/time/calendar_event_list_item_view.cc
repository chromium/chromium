// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_event_list_item_view.h"

#include <string>

#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_metrics.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/strings/utf_string_conversions.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace ash {
namespace {

// The paddings for `CalendarEventListViewItem`.
constexpr auto kEventListItemInsets = gfx::Insets::VH(0, 20);

// Paddings in this view.
constexpr int kEntryHorizontalPadding = 20;

// Radius of the event color dot.
constexpr int kColorDotRadius = 4;

// Dimension of the event color dot view.
constexpr int kColorDotViewSize = 8;

// Default Calendar API color ID to use when no event color is specifified.
constexpr char kDefaultColorId[] = "7";

// Map of Calendar API color ids and their respective hex color code.
std::map<std::string, std::string> event_hex_color_codes = {
    {"1", "a4bdfc"}, {"2", "7ae7bf"},  {"3", "dbadff"}, {"4", "ff887c"},
    {"5", "fbd75b"}, {"6", "ffb878"},  {"7", "46d6db"}, {"8", "e1e1e1"},
    {"9", "5484ed"}, {"10", "51b749"}, {"11", "dc2127"}};

// Sets up the event label.
void SetUpLabel(views::Label* label,
                gfx::ElideBehavior elide_behavior,
                gfx::HorizontalAlignment horizontal_alignment) {
  label->SetHorizontalAlignment(horizontal_alignment);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetElideBehavior(elide_behavior);
  label->SetSubpixelRenderingEnabled(false);
  label->SetTextContext(CONTEXT_CALENDAR_DATE);
}

// Renders an Event color dot.
class CalendarEventListItemDot : public views::View {
 public:
  CalendarEventListItemDot(std::string color_id) {
    DCHECK(color_id.empty() || event_hex_color_codes.count(color_id));
    std::string hex_code =
        event_hex_color_codes[color_id.empty() ? kDefaultColorId : color_id];
    base::HexStringToInt(hex_code, &color_);
  }
  ~CalendarEventListItemDot() override = default;
  CalendarEventListItemDot(const CalendarEventListItemDot& other) = delete;
  CalendarEventListItemDot& operator=(const CalendarEventListItemDot& other) =
      delete;

  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(kColorDotViewSize, kColorDotViewSize);
  }

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

}  // namespace

CalendarEventListItemView::CalendarEventListItemView(
    CalendarViewController* calendar_view_controller,
    google_apis::calendar::CalendarEvent event)
    : ActionableView(TrayPopupInkDropStyle::FILL_BOUNDS),
      calendar_view_controller_(calendar_view_controller),
      summary_(new views::Label()),
      time_range_(new views::Label()),
      event_url_(event.html_link()) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  DCHECK(calendar_view_controller_->selected_date().has_value());

  const base::Time event_start_time = event.start_time().date_time();
  const base::Time event_end_time = event.end_time().date_time();

  const base::TimeDelta time_difference = calendar_utils::GetTimeDifference(
      calendar_view_controller_->selected_date().value());

  const base::Time selected_midnight =
      calendar_view_controller_->selected_date_midnight();
  const base::Time selected_midnight_utc =
      calendar_view_controller_->selected_date_midnight_utc();
  const base::Time selected_last_minute =
      calendar_utils::GetNextDayMidnight(selected_midnight) - base::Minutes(1);
  const base::Time selected_last_minute_utc =
      selected_last_minute - time_difference;

  base::Time start_time =
      calendar_utils::GetMaxTime(event_start_time, selected_midnight_utc);
  base::Time end_time =
      calendar_utils::GetMinTime(event_end_time, selected_last_minute_utc);

  bool use_12_hour_clock =
      Shell::Get()->system_tray_model()->clock()->hour_clock_type() ==
      base::k12HourClock;
  std::u16string start_time_string =
      use_12_hour_clock
          ? calendar_utils::GetTwelveHourClockTime(start_time)
          : calendar_utils::GetTwentyFourHourClockTime(start_time);
  std::u16string end_time_string =
      use_12_hour_clock ? calendar_utils::GetTwelveHourClockTime(end_time)
                        : calendar_utils::GetTwentyFourHourClockTime(end_time);
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kButton);
  SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_ASH_CALENDAR_EVENT_ENTRY_ACCESSIBLE_DESCRIPTION, start_time_string,
      end_time_string, calendar_utils::GetTimeZone(start_time),
      base::UTF8ToUTF16(event.summary())));
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetBorder(views::CreateEmptyBorder(kEventListItemInsets));
  summary_->SetText(event.summary().empty()
                        ? l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_NO_TITLE)
                        : base::UTF8ToUTF16(event.summary()));
  SetUpLabel(summary_, gfx::ElideBehavior::ELIDE_TAIL,
             gfx::HorizontalAlignment::ALIGN_LEFT);
  summary_->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::VH(0, kEntryHorizontalPadding)));

  auto formatted_interval =
      use_12_hour_clock ? calendar_utils::FormatTwelveHourClockTimeInterval(
                              start_time, end_time)
                        : calendar_utils::FormatTwentyFourHourClockTimeInterval(
                              start_time, end_time);
  time_range_->SetText(formatted_interval);
  SetUpLabel(time_range_, gfx::NO_ELIDE,
             gfx::HorizontalAlignment::ALIGN_CENTER);

  // Creates a `TriView` which carries the `color_dot`, `summary_` and
  // `time_range_`.
  TriView* tri_view = TrayPopupUtils::CreateDefaultRowView();
  tri_view->SetMinSize(
      TriView::Container::START,
      gfx::Size(kColorDotViewSize,
                tri_view->GetMinSize(TriView::Container::START).height()));
  tri_view->AddView(TriView::Container::START,
                    AddChildView(std::make_unique<CalendarEventListItemDot>(
                        event.color_id())));
  tri_view->AddView(TriView::Container::CENTER, summary_);
  tri_view->AddView(TriView::Container::END, time_range_);

  auto tooltip_text = l10n_util::GetStringFUTF16(
      IDS_ASH_CALENDAR_EVENT_ENTRY_TOOL_TIP, base::UTF8ToUTF16(event.summary()),
      formatted_interval);
  time_range_->SetTooltipText(tooltip_text);
  summary_->SetTooltipText(tooltip_text);

  AddChildView(tri_view);
}

CalendarEventListItemView::~CalendarEventListItemView() = default;

void CalendarEventListItemView::OnThemeChanged() {
  views::View::OnThemeChanged();
  summary_->SetEnabledColor(calendar_utils::GetPrimaryTextColor());
  time_range_->SetEnabledColor(calendar_utils::GetPrimaryTextColor());
}

bool CalendarEventListItemView::PerformAction(const ui::Event& event) {
  DCHECK(event_url_.is_empty() || event_url_.is_valid());

  calendar_metrics::RecordEventListItemActivated(event);
  calendar_view_controller_->OnCalendarEventWillLaunch();

  GURL finalized_url;
  bool opened_pwa = false;
  DCHECK(calendar_view_controller_->selected_date().has_value());
  Shell::Get()->system_tray_model()->client()->ShowCalendarEvent(
      event_url_, calendar_view_controller_->selected_date_midnight(),
      opened_pwa, finalized_url);
  return true;
}

BEGIN_METADATA(CalendarEventListItemView, views::View);
END_METADATA

}  // namespace ash
