// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_event_list_view.h"

#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/highlight_border.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/i18n/time_formatting.h"
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
#include "ui/views/layout/box_layout.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace ash {
namespace {

// The paddings in `CalendarEventListView`.
constexpr gfx::Insets kContentInsets{20, 20, 0, 20};

// The insets for `CalendarEmptyEventListView` label.
constexpr gfx::Insets kOpenGoogleCalendarInsets{6, 16};

// The insets for `CalendarEmptyEventListView`.
constexpr gfx::Insets kOpenGoogleCalendarContainerInsets{20, 60};

}  // namespace

// A view that's displayed when the user selects a day cell from the calendar
// month view that has no events.  Clicking on it opens Google calendar.
class CalendarEmptyEventListView : public views::LabelButton {
 public:
  explicit CalendarEmptyEventListView(CalendarViewController* controller)
      : views::LabelButton(
            views::Button::PressedCallback(base::BindRepeating(
                &CalendarEmptyEventListView::OpenCalendarDefault,
                base::Unretained(this))),
            l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_NO_EVENTS)),
        controller_(controller) {
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
    label()->SetBorder(views::CreateEmptyBorder(kOpenGoogleCalendarInsets));
    label()->SetTextContext(CONTEXT_CALENDAR_DATE);
    SetBorder(std::make_unique<HighlightBorder>(
        GetPreferredSize().height() / 2,
        HighlightBorder::Type::kHighlightBorder1,
        /*use_light_colors=*/true));
    SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_NO_EVENT_BUTTON_TOOL_TIP));
  }
  CalendarEmptyEventListView(const CalendarEmptyEventListView& other) = delete;
  CalendarEmptyEventListView& operator=(
      const CalendarEmptyEventListView& other) = delete;
  ~CalendarEmptyEventListView() override = default;

  // views::View:
  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    SetEnabledTextColors(calendar_utils::GetPrimaryTextColor());
  }

  // Callback that's invoked when the user clicks on "Open in Google calendar"
  // in an empty event list.
  void OpenCalendarDefault() {
    controller_->OnCalendarEventWillLaunch();

    GURL finalized_url;
    bool opened_pwa = false;
    Shell::Get()->system_tray_model()->client()->ShowCalendarEvent(
        absl::nullopt, opened_pwa, finalized_url);
  }

 private:
  // Owned by the parent view. Guaranteed to outlive this.
  CalendarViewController* const controller_;
};

CalendarEventListView::CalendarEventListView(
    CalendarViewController* calendar_view_controller)
    : calendar_view_controller_(calendar_view_controller),
      close_button_(AddChildView(std::make_unique<views::ImageButton>(
          views::Button::PressedCallback(base::BindRepeating(
              &CalendarViewController::CloseEventListView,
              base::Unretained(calendar_view_controller)))))),
      scroll_view_(AddChildView(std::make_unique<views::ScrollView>())),
      content_view_(
          scroll_view_->SetContents(std::make_unique<views::View>())) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // Set up background color and blur.
  auto* color_provider = AshColorProvider::Get();
  SkColor background_color = color_provider->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kOpaque);
  SetBackground(views::CreateSolidBackground(background_color));
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);

  close_button_->SetImage(
      views::ImageButton::STATE_NORMAL,
      gfx::CreateVectorIcon(views::kIcCloseIcon,
                            calendar_utils::GetPrimaryTextColor()));
  close_button_->SetImageHorizontalAlignment(views::ImageButton::ALIGN_RIGHT);
  close_button_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  close_button_->SetBorder(views::CreateEmptyBorder(kContentInsets));
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_CLOSE_BUTTON_ACCESSIBLE_DESCRIPTION));
  close_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_CLOSE_BUTTON_TOOLTIP));
  close_button_->SetFocusBehavior(FocusBehavior::ALWAYS);

  scroll_view_->SetAllowKeyboardScrolling(false);
  scroll_view_->SetBackgroundColor(absl::nullopt);
  // Gives a min height so the background color can be filled to all the spaces
  // in the available expanded area.
  scroll_view_->ClipHeightTo(
      calendar_view_controller_->expanded_area_available_height() -
          close_button_->GetPreferredSize().height(),
      INT_MAX);
  scroll_view_->SetDrawOverflowIndicator(false);
  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);

  content_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  content_view_->SetBorder(views::CreateEmptyBorder(kContentInsets));

  UpdateListItems();

  scoped_calendar_view_controller_observer_.Observe(calendar_view_controller_);
}

CalendarEventListView::~CalendarEventListView() = default;

void CalendarEventListView::OnSelectedDateUpdated() {
  UpdateListItems();
}

void CalendarEventListView::UpdateListItems() {
  content_view_->RemoveAllChildViews();

  calendar_view_controller_->MaybeUpdateTimeDifference(
      calendar_view_controller_->selected_date().value());

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
      base::TimeFormatWithPattern(
          calendar_view_controller_->selected_date().value(), "MMMMd")));
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
