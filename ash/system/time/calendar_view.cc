// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_view.h"

#include "ash/public/cpp/ash_typography.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/time/calendar_month_view.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// The paddings in each view.
constexpr gfx::Insets kContentInsets{20};
constexpr int kContentVerticalPadding = 20;
constexpr int kMonthVerticalPadding = 10;
constexpr int kLabelVerticalPadding = 10;

// The pixel that will be applied to indicate that we can see this is the view's
// bottom if there's this much pixel left.
constexpr int kPrepareEndOfView = 30;

}  // namespace

CalendarView::CalendarView(DetailedViewDelegate* delegate,
                           CalendarViewController* calendar_view_controller)
    : TrayDetailedView(delegate),
      calendar_view_controller_(calendar_view_controller),
      scroll_view_(AddChildView(std::make_unique<views::ScrollView>())),
      content_view_(
          scroll_view_->SetContents(std::make_unique<views::View>())) {
  CreateTitleRow(IDS_ASH_CALENDAR_TITLE);
  // TODO(jiamingc@): add header.
  scroll_view_->SetBackgroundColor(absl::nullopt);
  scroll_view_->ClipHeightTo(0, INT_MAX);
  scroll_view_->SetDrawOverflowIndicator(false);
  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);

  content_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  content_view_->SetBorder(views::CreateEmptyBorder(kContentInsets));

  SetMonthViews();

  scoped_scroll_view_observer_.Observe(scroll_view_);
  scoped_calendar_view_controller_observer_.Observe(calendar_view_controller_);
}

CalendarView::~CalendarView() = default;

void CalendarView::Init() {
  // `Layout` the view first then auto scroll to `PositionOfToday`, otherwise
  // the auto scroll won't work.
  Layout();

  is_resetting_scroll_ = true;
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                 PositionOfToday());
  is_resetting_scroll_ = false;
}

void CalendarView::SetMonthViews() {
  previous_label_ =
      AddLabelWithId(calendar_view_controller_->GetPreviousMonthName());
  previous_month_ =
      AddMonth(calendar_view_controller_->GetPreviousMonthFirstDay());

  current_label_ =
      AddLabelWithId(calendar_view_controller_->GetOnScreenMonthName());
  current_month_ =
      AddMonth(calendar_view_controller_->GetOnScreenMonthFirstDay());

  next_label_ = AddLabelWithId(calendar_view_controller_->GetNextMonthName());
  next_month_ = AddMonth(calendar_view_controller_->GetNextMonthFirstDay());
}

int CalendarView::PositionOfToday() {
  return kContentVerticalPadding +
         previous_label_->GetPreferredSize().height() +
         previous_month_->GetPreferredSize().height() +
         current_label_->GetPreferredSize().height();
}

void CalendarView::OnThemeChanged() {
  views::View::OnThemeChanged();
  const SkColor primary_text_color =
      ash::AshColorProvider::Get()->GetContentLayerColor(
          ash::AshColorProvider::ContentLayerType::kTextColorPrimary);
  previous_label_->SetEnabledColor(primary_text_color);
  current_label_->SetEnabledColor(primary_text_color);
  next_label_->SetEnabledColor(primary_text_color);
}

views::Label* CalendarView::AddLabelWithId(std::u16string label_string,
                                           bool add_at_front) {
  const SkColor primary_text_color =
      ash::AshColorProvider::Get()->GetContentLayerColor(
          ash::AshColorProvider::ContentLayerType::kTextColorPrimary);
  auto label = std::make_unique<views::Label>();
  label->SetText(label_string);
  label->SetBorder(views::CreateEmptyBorder(kLabelVerticalPadding, 0,
                                            kLabelVerticalPadding, 0));
  label->SetEnabledColor(primary_text_color);
  label->SetTextContext(CONTEXT_CALENDAR_LABEL);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);
  if (add_at_front) {
    return content_view_->AddChildViewAt(std::move(label), 0);
  } else {
    return content_view_->AddChildView(std::move(label));
  }
}

CalendarMonthView* CalendarView::AddMonth(base::Time month_first_date,
                                          bool add_at_front) {
  auto month = std::make_unique<CalendarMonthView>(month_first_date);
  month->SetBorder(views::CreateEmptyBorder(kMonthVerticalPadding, 0,
                                            kMonthVerticalPadding, 0));
  if (add_at_front) {
    return content_view_->AddChildViewAt(std::move(month), 0);
  } else {
    return content_view_->AddChildView(std::move(month));
  }
}

void CalendarView::OnContentsScrolled() {
  // The scroll position is reset because it's adjusting the position when
  // adding or removing views from the `scroll_view_`. It should scroll to the
  // position we want, so we don't need to check the visible area position.
  if (is_resetting_scroll_)
    return;

  // Scrolls to the previous month if the current label is moving down and
  // passing the top of the visible area.
  if (scroll_view_->GetVisibleRect().y() <= current_label_->y()) {
    ScrollUpOneMonth();

    // After adding a new month in the content, the current position stays the
    // same but below the added view the each view's position has changed to
    // [original position + new month's height]. So we need to add the height of
    // the newly added month to keep the current view's position.
    int added_height = previous_month_->GetPreferredSize().height() +
                       previous_label_->GetPreferredSize().height();
    int position = added_height + scroll_view_->GetVisibleRect().y();
    is_resetting_scroll_ = true;
    scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                   position);
    is_resetting_scroll_ = false;
  } else if (scroll_view_->GetVisibleRect().y() >= next_label_->y() ||
             scroll_view_->GetVisibleRect().y() +
                     scroll_view_->GetVisibleRect().height() >
                 next_month_->y() + next_month_->height() - kPrepareEndOfView) {
    // Renders the next month if the next month label is moving up and passing
    // the top of the visible area, or the next month body's bottom is passing
    // the bottom of the visible area.
    int removed_height = previous_month_->GetPreferredSize().height() +
                         previous_label_->GetPreferredSize().height();
    ScrollDownOneMonth();

    // Same as adding previous views. We need to remove the height of the
    // deleted month to keep the current view's position.
    int position = scroll_view_->GetVisibleRect().y() - removed_height;
    is_resetting_scroll_ = true;
    scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                   position);
    is_resetting_scroll_ = false;
  }
}

void CalendarView::OnMonthChanged(const base::Time::Exploded current_month) {
  // TODO(jiamingc@): update header.
}

void CalendarView::ScrollUpOneMonth() {
  calendar_view_controller_->UpdateMonth(
      calendar_view_controller_->GetPreviousMonthFirstDay());
  content_view_->RemoveChildViewT(next_label_);
  content_view_->RemoveChildViewT(next_month_);

  next_label_ = current_label_;
  next_month_ = current_month_;
  current_label_ = previous_label_;
  current_month_ = previous_month_;

  previous_month_ =
      AddMonth(calendar_view_controller_->GetPreviousMonthFirstDay(),
               /*add_at_front=*/true);
  previous_label_ = AddLabelWithId(
      calendar_view_controller_->GetPreviousMonthName(), /*add_at_front=*/true);
}

void CalendarView::ScrollDownOneMonth() {
  calendar_view_controller_->UpdateMonth(
      calendar_view_controller_->GetNextMonthFirstDay());
  content_view_->RemoveChildViewT(previous_label_);
  content_view_->RemoveChildViewT(previous_month_);

  previous_label_ = current_label_;
  previous_month_ = current_month_;
  current_label_ = next_label_;
  current_month_ = next_month_;

  next_label_ = AddLabelWithId(calendar_view_controller_->GetNextMonthName());
  next_month_ = AddMonth(calendar_view_controller_->GetNextMonthFirstDay());
}

BEGIN_METADATA(CalendarView, views::View)
END_METADATA

}  // namespace ash
