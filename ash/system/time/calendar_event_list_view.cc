// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_event_list_view.h"

#include "ash/bubble/bubble_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "calendar_event_list_item_view.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace ash {
namespace {

// The paddings in `close_button_container_`.
constexpr gfx::Insets kCloseButtonContainerInsets{15};

// The paddings in `CalendarEventListView`.
constexpr auto kContentInsets = gfx::Insets::TLBR(0, 0, 20, 0);

// The insets for `CalendarEmptyEventListView` label.
constexpr auto kOpenGoogleCalendarInsets = gfx::Insets::VH(6, 16);

// The insets for `CalendarEmptyEventListView`.
constexpr auto kOpenGoogleCalendarContainerInsets = gfx::Insets::VH(20, 80);

// Border thickness for `CalendarEmptyEventListView`.
constexpr int kOpenGoogleCalendarBorderThickness = 1;

}  // namespace

// A view that's displayed when the user selects a day cell from the calendar
// month view that has no events. Clicking on it opens Google calendar.
class CalendarEmptyEventListView : public PillButton {
 public:
  explicit CalendarEmptyEventListView(CalendarViewController* controller)
      : PillButton(views::Button::PressedCallback(base::BindRepeating(
                       &CalendarEmptyEventListView::OpenCalendarDefault,
                       base::Unretained(this))),
                   l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_NO_EVENTS),
                   PillButton::Type::kIconlessFloating,
                   /*icon=*/nullptr),
        controller_(controller) {
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
    label()->SetBorder(views::CreateEmptyBorder(kOpenGoogleCalendarInsets));
    label()->SetTextContext(CONTEXT_CALENDAR_DATE);
    SetBorder(views::CreateRoundedRectBorder(
        kOpenGoogleCalendarBorderThickness, GetPreferredSize().height() / 2,
        AshColorProvider::Get()->GetControlsLayerColor(
            ColorProvider::ControlsLayerType::kHairlineBorderColor)));
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

    GURL finalized_url;
    bool opened_pwa = false;
    DCHECK(controller_->selected_date().has_value());

    // Open Google calendar and land on the local day/month/year.
    Shell::Get()->system_tray_model()->client()->ShowCalendarEvent(
        absl::nullopt, controller_->selected_date_midnight(), opened_pwa,
        finalized_url);
  }

 private:
  // Owned by the parent view. Guaranteed to outlive this.
  CalendarViewController* const controller_;
};

CalendarEventListView::CalendarEventListView(
    CalendarViewController* calendar_view_controller)
    : calendar_view_controller_(calendar_view_controller),
      close_button_container_(AddChildView(std::make_unique<views::View>())),
      scroll_view_(AddChildView(std::make_unique<views::ScrollView>())),
      content_view_(
          scroll_view_->SetContents(std::make_unique<views::View>())) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // Set up background color.
  auto* color_provider = AshColorProvider::Get();
  SkColor background_color = color_provider->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kOpaque);
  SetBackground(views::CreateSolidBackground(background_color));
  SetPaintToLayer();

  // Set the bottom corners to be rounded so that `CalendarEventListView` is
  // contained in `CalendarView`.
  layer()->SetRoundedCornerRadius(
      {0, 0, kBubbleCornerRadius, kBubbleCornerRadius});

  views::BoxLayout* button_layout = close_button_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  button_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  close_button_container_->SetBorder(
      views::CreateEmptyBorder(kCloseButtonContainerInsets));

  auto* close_button =
      new IconButton(views::Button::PressedCallback(base::BindRepeating(
                         &CalendarViewController::CloseEventListView,
                         base::Unretained(calendar_view_controller))),
                     IconButton::Type::kSmallFloating, &views::kIcCloseIcon,
                     IDS_ASH_CLOSE_BUTTON_ACCESSIBLE_DESCRIPTION);
  close_button_container_->AddChildView(close_button);

  scroll_view_->SetAllowKeyboardScrolling(false);
  scroll_view_->SetBackgroundColor(absl::nullopt);
  // Gives a min height so the background color can be filled to all the spaces
  // in the available expanded area.
  scroll_view_->ClipHeightTo(
      INT_MAX - close_button_container_->GetPreferredSize().height(), INT_MAX);
  scroll_view_->SetDrawOverflowIndicator(false);
  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);

  content_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  content_view_->SetBorder(views::CreateEmptyBorder(kContentInsets));

  UpdateListItems();

  scoped_calendar_view_controller_observer_.Observe(calendar_view_controller_);
  scoped_calendar_model_observer_.Observe(
      Shell::Get()->system_tray_model()->calendar_model());
}

CalendarEventListView::~CalendarEventListView() = default;

void CalendarEventListView::OnSelectedDateUpdated() {
  UpdateListItems();
}

void CalendarEventListView::OnEventsFetched(
    const CalendarModel::FetchingStatus status,
    const base::Time start_time,
    const google_apis::calendar::EventList* events) {
  if (status == CalendarModel::kSuccess &&
      start_time == calendar_utils::GetStartOfMonthUTC(
                        calendar_view_controller_->selected_date_midnight())) {
    UpdateListItems();
  }
}

void CalendarEventListView::UpdateListItems() {
  content_view_->RemoveAllChildViews();

  std::list<google_apis::calendar::CalendarEvent> events =
      calendar_view_controller_->SelectedDateEvents();

  if (events.size() > 0) {
    // Sorts the event by start time.
    events.sort([](google_apis::calendar::CalendarEvent& a,
                   google_apis::calendar::CalendarEvent& b) {
      return a.start_time().date_time() < b.start_time().date_time();
    });

    for (const google_apis::calendar::CalendarEvent& event : events) {
      auto* event_entry = content_view_->AddChildView(
          std::make_unique<CalendarEventListItemView>(calendar_view_controller_,
                                                      event));

      // Needs to repaint the `content_view_`'s children.
      event_entry->InvalidateLayout();
    }

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
  DCHECK(calendar_view_controller_->selected_date().has_value());
  empty_button->SetAccessibleName(l10n_util::GetStringFUTF16(
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

BEGIN_METADATA(CalendarEventListView, views::View);
END_METADATA

}  // namespace ash
