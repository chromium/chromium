// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_event_list_view.h"

#include <memory>

#include "ash/bubble/bubble_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_event_list_item_view.h"
#include "ash/system/time/calendar_metrics.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/constants/chromeos_features.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace ash {
namespace {

// The paddings in `close_button_container_`.
const auto kCloseButtonContainerInsets = gfx::Insets::VH(8, 16);

// The paddings in `CalendarEventListView`.
constexpr auto kContentInsets = gfx::Insets::TLBR(0, 16, 16, 16);

// The insets for `CalendarEmptyEventListView`.
constexpr auto kOpenGoogleCalendarContainerInsets = gfx::Insets::VH(20, 60);

// Border thickness for `CalendarEmptyEventListView`.
constexpr int kOpenGoogleCalendarBorderThickness = 1;

constexpr auto kDeprecatedEventListViewCornerRadius =
    gfx::RoundedCornersF(24,
                         24,
                         kDeprecatedBubbleCornerRadius,
                         kDeprecatedBubbleCornerRadius);

constexpr auto kEventListViewCornerRadius =
    gfx::RoundedCornersF(kUpdatedBubbleCornerRadius);

constexpr int kScrollViewGradientSize = 16;

// The spacing between the child lists where we separate multi-day and non
// multi-day events into two separate child list views.
constexpr int kEventListViewBetweenChildSpacing = 8;

// The between child spacing within the child event lists.
constexpr int kChildEventListBetweenChildSpacing = 2;

}  // namespace

// A view that's displayed when the user selects a day cell from the calendar
// month view that has no events. Clicking on it opens Google calendar.
class CalendarEmptyEventListView : public PillButton {
  METADATA_HEADER(CalendarEmptyEventListView, PillButton)

 public:
  explicit CalendarEmptyEventListView(CalendarViewController* controller)
      : PillButton(views::Button::PressedCallback(base::BindRepeating(
                       &CalendarEmptyEventListView::OpenCalendarDefault,
                       base::Unretained(this))),
                   l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_NO_EVENTS),
                   PillButton::Type::kSecondaryWithoutIcon,
                   /*icon=*/nullptr),
        controller_(controller) {
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);

    SetBorder(views::CreateThemedRoundedRectBorder(
        kOpenGoogleCalendarBorderThickness, GetPreferredSize().height() / 2,
        kColorAshHairlineBorderColor));
    SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_NO_EVENT_BUTTON_TOOL_TIP));
  }
  CalendarEmptyEventListView(const CalendarEmptyEventListView& other) = delete;
  CalendarEmptyEventListView& operator=(
      const CalendarEmptyEventListView& other) = delete;
  ~CalendarEmptyEventListView() override = default;

  // Callback that's invoked when the user clicks on "Open in Google calendar"
  // in an empty event list.
  void OpenCalendarDefault() {
    controller_->OnCalendarEventWillLaunch();

    calendar_metrics::RecordCalendarLaunchedFromEmptyEventList();

    GURL finalized_url;
    bool opened_pwa = false;
    DCHECK(controller_->selected_date().has_value());

    // Open Google calendar and land on the local day/month/year.
    Shell::Get()->system_tray_model()->client()->ShowCalendarEvent(
        std::nullopt, controller_->selected_date_midnight(), opened_pwa,
        finalized_url);
  }

 private:
  // Owned by the parent view. Guaranteed to outlive this.
  const raw_ptr<CalendarViewController> controller_;
};

BEGIN_METADATA(CalendarEmptyEventListView)
END_METADATA

CalendarEventListView::CalendarEventListView(
    CalendarViewController* calendar_view_controller)
    : calendar_view_controller_(calendar_view_controller),
      close_button_container_(AddChildView(std::make_unique<views::View>())),
      scroll_view_(AddChildView(std::make_unique<views::ScrollView>())),
      content_view_(
          scroll_view_->SetContents(std::make_unique<views::View>())) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  // Set the bottom corners to be rounded so that `CalendarEventListView` is
  // contained in `CalendarView`.
  layer()->SetRoundedCornerRadius(features::IsBubbleCornerRadiusUpdateEnabled()
                                      ? kEventListViewCornerRadius
                                      : kDeprecatedEventListViewCornerRadius);

  views::BoxLayout* button_layout = close_button_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  button_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  close_button_container_->SetBorder(
      views::CreateEmptyBorder(kCloseButtonContainerInsets));

  close_button_ =
      close_button_container_->AddChildView(std::make_unique<IconButton>(
          views::Button::PressedCallback(
              base::BindRepeating(&CalendarViewController::CloseEventListView,
                                  base::Unretained(calendar_view_controller))),
          IconButton::Type::kMediumFloating, &views::kIcCloseIcon,
          IDS_ASH_CLOSE_BUTTON_ACCESSIBLE_DESCRIPTION));

  scroll_view_->SetAllowKeyboardScrolling(false);
  scroll_view_->SetBackgroundColor(std::nullopt);
  // Gives a min height so the background color can be filled to all the spaces
  // in the available expanded area.
  scroll_view_->ClipHeightTo(
      INT_MAX - close_button_container_->GetPreferredSize().height(), INT_MAX);
  scroll_view_->SetDrawOverflowIndicator(false);
  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);

  // Set up fade in/fade out gradients at top/bottom of scroll view.
  scroll_view_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  gradient_helper_ = std::make_unique<ScrollViewGradientHelper>(
      scroll_view_, kScrollViewGradientSize);

  content_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kEventListViewBetweenChildSpacing));
  content_view_->SetBorder(views::CreateEmptyBorder(kContentInsets));

  UpdateListItems();

  scoped_calendar_view_controller_observer_.Observe(
      calendar_view_controller_.get());
  scoped_calendar_model_observer_.Observe(
      Shell::Get()->system_tray_model()->calendar_model());
}

CalendarEventListView::~CalendarEventListView() = default;

void CalendarEventListView::OnThemeChanged() {
  views::View::OnThemeChanged();
  SetBackground(views::CreateSolidBackground(
      GetColorProvider()->GetColor(cros_tokens::kCrosSysSystemOnBaseOpaque)));
}

void CalendarEventListView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);

  if (gradient_helper_) {
    gradient_helper_->UpdateGradientMask();
  }

  // If `CalendarEventListItemView` or the join button is focused, do not scroll
  // to the current or next event. Otherwise `scroll_view_` won't scroll with
  // the focus change.
  if (GetFocusManager() && GetFocusManager()->GetFocusedView()) {
    const auto focused_view_class_name =
        std::string_view(GetFocusManager()->GetFocusedView()->GetClassName());
    if (focused_view_class_name ==
            std::string_view(CalendarEventListItemView::kViewClassName) ||
        focused_view_class_name ==
            std::string_view(PillButton::kViewClassName)) {
      return;
    }
  }

  const std::optional<base::Time> selected_date =
      calendar_view_controller_->selected_date();

  // If the selected date is not today, do not auto scroll and reset the
  // `scroll_view_` position. Otherwise the previous position will be preserved.
  if (!calendar_utils::IsToday(selected_date.value())) {
    scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(), 0);
    return;
  }

  // Scrolls to the top of `current_or_next_event_view_`. Ignores the multi-day
  // events on the top if exists.
  if (current_or_next_event_view_) {
    auto* multi_day_events_container =
        GetViewByID(kEventListMultiDayEventsContainer);

    scroll_view_->ScrollToPosition(
        scroll_view_->vertical_scroll_bar(),
        (multi_day_events_container
             ? multi_day_events_container->GetPreferredSize().height() +
                   kEventListViewBetweenChildSpacing
             : 0) +
            (current_or_next_event_view_->GetPreferredSize().height() +
             kChildEventListBetweenChildSpacing) *
                current_or_next_event_index_);
  } else {
    // If there's no current or next event because there's no single-day event
    // for today or all events have passed, scroll to the end of the list if
    // selected date is today.
    scroll_view_->ScrollToPosition(
        scroll_view_->vertical_scroll_bar(),
        scroll_view_->GetVisibleRect().bottom() + kContentInsets.bottom());
  }
}

void CalendarEventListView::RequestCloseButtonFocus() {
  close_button_->RequestFocus();
}

void CalendarEventListView::OnSelectedDateUpdated() {
  UpdateListItems();
}

void CalendarEventListView::OnEventsFetched(
    const CalendarModel::FetchingStatus status,
    const base::Time start_time) {
  if (status == CalendarModel::kSuccess &&
      start_time == calendar_utils::GetStartOfMonthUTC(
                        calendar_view_controller_->selected_date_midnight())) {
    UpdateListItems();
  }
}

std::unique_ptr<views::View> CalendarEventListView::CreateChildEventListView(
    std::list<google_apis::calendar::CalendarEvent> events,
    int parent_view_id) {
  auto container = std::make_unique<views::View>();
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kChildEventListBetweenChildSpacing));
  container->SetID(parent_view_id);

  const int events_size = events.size();
  for (SingleDayEventList::iterator it = events.begin(); it != events.end();
       ++it) {
    const int event_index = std::distance(events.begin(), it) + 1;
    auto* event_list_item_view =
        container->AddChildView(std::make_unique<CalendarEventListItemView>(
            /*calendar_view_controller=*/calendar_view_controller_,
            /*selected_date_params=*/
            SelectedDateParams{
                calendar_view_controller_->selected_date().value(),
                calendar_view_controller_->selected_date_midnight(),
                calendar_view_controller_
                    ->selected_date_midnight_utc()}, /*event=*/
            *it,
            /*ui_params=*/
            UIParams{/*round_top_corners=*/it == events.begin(),
                     /*round_bottom_corners=*/it->id() == events.rbegin()->id(),
                     /*is_up_next_event_list_item=*/false,
                     /*show_event_list_dot=*/true,
                     /*fixed_width=*/0},
            /*event_list_item_index=*/
            EventListItemIndex{/*item_index=*/event_index,
                               /*total_count_of_events=*/events_size}));

    // The `current_or_next_event_view_` is the first event that is not an
    // all-day or multi-day event, and is the ongoing or the following event.
    if (!current_or_next_event_view_ &&
        event_list_item_view->is_current_or_next_single_day_event()) {
      current_or_next_event_view_ = event_list_item_view;
      current_or_next_event_index_ = event_index - 1;
    }
  }

  return container;
}

void CalendarEventListView::UpdateListItems() {
  // Resets `current_or_next_event_view_` and `current_or_next_event_index_`
  // since the `event_list_view_` has been updated. This has to be reset before
  // `RemoveAllChildViews()` is called otherwise it will become a dangling ptr.
  current_or_next_event_view_ = nullptr;
  current_or_next_event_index_ = 0;

  content_view_->RemoveAllChildViews();

  const auto [multi_day_events, all_other_events] =
      calendar_view_controller_->SelectedDateEventsSplitByMultiDayAndSameDay();

  // If we have some events to display, then add them to the `content_view_`
  // and early return (the following methods in `UpdateListItems` handle empty
  // state etc).
  if (!multi_day_events.empty()) {
    content_view_->AddChildView(CreateChildEventListView(
        multi_day_events, kEventListMultiDayEventsContainer));
  }
  if (!all_other_events.empty()) {
    content_view_->AddChildView(CreateChildEventListView(
        all_other_events, kEventListSameDayEventsContainer));
  }

  content_view_->InvalidateLayout();

  calendar_metrics::RecordEventListEventCount(multi_day_events.size() +
                                              all_other_events.size());

  if (!multi_day_events.empty() || !all_other_events.empty()) {
    return;
  }

  // Show "Open in Google calendar"
  auto empty_list_view_container = std::make_unique<views::View>();
  empty_list_view_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  CalendarEmptyEventListView* empty_button =
      empty_list_view_container->AddChildView(
          std::make_unique<CalendarEmptyEventListView>(
              calendar_view_controller_));

  // There is a corner case when user closes the event list view before this
  // line of code is executed. Then `selected_date_` is std::nullopt and
  // getting its value leads to a crash. Only set accessible name when
  // `selected_date_` has value, since if `event_list_view_` is closed, there'll
  // be no need to set the accessible name.
  if (!calendar_view_controller_->selected_date().has_value()) {
    return;
  }
  empty_button->GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
      IDS_ASH_CALENDAR_NO_EVENT_BUTTON_ACCESSIBLE_DESCRIPTION,
      calendar_utils::GetMonthNameAndDayOfMonth(
          calendar_view_controller_->selected_date().value())));
  empty_list_view_container->SetBorder(
      views::CreateEmptyBorder(kOpenGoogleCalendarContainerInsets));
  views::View* empty_list_view =
      content_view_->AddChildView(std::move(empty_list_view_container));

  // Needs to repaint the `content_view_`'s children.
  empty_list_view->InvalidateLayout();
}

BEGIN_METADATA(CalendarEventListView);
END_METADATA

}  // namespace ash
