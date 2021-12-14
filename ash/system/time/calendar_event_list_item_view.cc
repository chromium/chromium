// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_event_list_item_view.h"

#include "ash/public/cpp/ash_typography.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "google_apis/calendar/calendar_api_response_types.h"
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

// The meeting title is too long, it is truncated in this length.
constexpr int kTruncatedTitleLength = 20;

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
void SetUpLabel(views::Label* label) {
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetElideBehavior(gfx::NO_ELIDE);
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
    google_apis::calendar::CalendarEvent event)
    : ActionableView(TrayPopupInkDropStyle::FILL_BOUNDS),
      summary_(new views::Label()),
      time_range_(new views::Label()) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  GetViewAccessibility().OverrideName(GetClassName());
  SetFocusBehavior(FocusBehavior::ALWAYS);

  summary_->SetText(base::UTF8ToUTF16(event.summary()));
  SetUpLabel(summary_);
  summary_->SetTruncateLength(kTruncatedTitleLength);
  summary_->SetBorder(
      views::CreateEmptyBorder(0, kEntryHorizontalPadding, 0, 0));

  auto start_time = event.start_time().date_time();
  auto end_time = event.end_time().date_time();
  auto time_string = base::TimeFormatWithPattern(start_time, "h:mm a") +
                     u" - " + base::TimeFormatWithPattern(end_time, "h:mm a");

  time_range_->SetText(time_string);
  SetUpLabel(time_range_);

  // Creates a `TriView` which carries the `color_dot` and `summary_` at the
  // entry start and the `time_range_` at the entry end.
  TriView* tri_view = TrayPopupUtils::CreateDefaultRowView();
  tri_view->AddView(TriView::Container::START,
                    AddChildView(std::make_unique<CalendarEventListItemDot>(
                        event.color_id())));
  tri_view->AddView(TriView::Container::START, summary_);
  tri_view->AddView(TriView::Container::END, time_range_);

  AddChildView(tri_view);
}

CalendarEventListItemView::~CalendarEventListItemView() = default;

void CalendarEventListItemView::OnThemeChanged() {
  views::View::OnThemeChanged();
  summary_->SetEnabledColor(calendar_utils::GetPrimaryTextColor());
  time_range_->SetEnabledColor(calendar_utils::GetPrimaryTextColor());
}

bool CalendarEventListItemView::PerformAction(const ui::Event& event) {
  // TODO(https://crbug.com/1270938): Launch web app implementation.
  return true;
}

BEGIN_METADATA(CalendarEventListItemView, views::View);
END_METADATA

}  // namespace ash
