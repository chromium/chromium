// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_view.h"

#include "ash/public/cpp/ash_typography.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/time/calendar_month_view.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "ash/system/unified/rounded_label_button.h"
#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace ash {
namespace {

// The paddings in each view.
constexpr int kContentVerticalPadding = 20;
constexpr int kContentHorizontalPadding = 20;
constexpr int kMonthVerticalPadding = 10;
constexpr int kLabelVerticalPadding = 10;
constexpr int kLabelTextInBetweenPadding = 10;

// The insets within the content view.
constexpr gfx::Insets kContentInsets{kContentVerticalPadding};

// The pixel that will be applied to indicate that we can see this is the view's
// bottom if there's this much pixel left.
constexpr int kPrepareEndOfView = 30;

// TODO(https://crbug.com/1236276): for some language it may start from "M".
constexpr int kDefaultWeekTitles[] = {
    IDS_ASH_CALENDAR_SUN, IDS_ASH_CALENDAR_MON, IDS_ASH_CALENDAR_TUE,
    IDS_ASH_CALENDAR_WED, IDS_ASH_CALENDAR_THU, IDS_ASH_CALENDAR_FRI,
    IDS_ASH_CALENDAR_SAT};

// The button on the header view.
class HeaderButton : public TopShortcutButton {
 public:
  HeaderButton(views::Button::PressedCallback callback,
               const gfx::VectorIcon& icon,
               int accessible_name_id)
      : TopShortcutButton(std::move(callback), icon, accessible_name_id) {}
  HeaderButton(const HeaderButton&) = delete;
  HeaderButton& operator=(const HeaderButton&) = delete;
  ~HeaderButton() override = default;

  // Does not need the background color from `TopShortcutButton`.
  void PaintButtonContents(gfx::Canvas* canvas) override {
    views::ImageButton::PaintButtonContents(canvas);
  }
};

// The overridden `Label` view used in `CalendarView`.
class CalendarLabel : public views::Label {
 public:
  explicit CalendarLabel(const std::u16string& text) : views::Label(text) {
    views::Label::SetEnabledColor(calendar_utils::GetPrimaryTextColor());
  }
  CalendarLabel(const CalendarLabel&) = delete;
  CalendarLabel& operator=(const CalendarLabel&) = delete;
  ~CalendarLabel() override = default;

  void OnThemeChanged() override {
    views::Label::OnThemeChanged();

    views::Label::SetEnabledColor(calendar_utils::GetPrimaryTextColor());
  }
};

// The month view header which contains the title of each week day.
class MonthHeaderView : public views::View {
 public:
  MonthHeaderView() {
    views::GridLayout* layout =
        SetLayoutManager(std::make_unique<views::GridLayout>());
    views::ColumnSet* column_set = layout->AddColumnSet(0);
    calendar_utils::SetUpWeekColumnSets(column_set);
    layout->StartRow(0, 0);

    for (int week_day : kDefaultWeekTitles) {
      auto label =
          std::make_unique<CalendarLabel>(l10n_util::GetStringUTF16(week_day));
      label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
      label->SetBorder(
          views::CreateEmptyBorder(calendar_utils::kDateCellInsets));
      label->SetElideBehavior(gfx::NO_ELIDE);
      label->SetSubpixelRenderingEnabled(false);
      label->SetFontList(views::Label::GetDefaultFontList().Derive(
          1, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));

      layout->AddView(std::move(label));
    }
  }

  MonthHeaderView(const MonthHeaderView& other) = delete;
  MonthHeaderView& operator=(const MonthHeaderView& other) = delete;
  ~MonthHeaderView() override = default;
};

}  // namespace

CalendarView::CalendarView(DetailedViewDelegate* delegate,
                           UnifiedSystemTrayController* controller,
                           CalendarViewController* calendar_view_controller)
    : TrayDetailedView(delegate),
      controller_(controller),
      calendar_view_controller_(calendar_view_controller) {
  CreateTitleRow(IDS_ASH_CALENDAR_TITLE);

  // Add the header.
  header_ = TrayPopupUtils::CreateDefaultLabel();
  header_->SetText(calendar_view_controller_->GetOnScreenMonthName());
  header_->SetTextContext(CONTEXT_CALENDAR_LABEL);
  header_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);

  header_year_ = TrayPopupUtils::CreateDefaultLabel();
  header_year_->SetText(base::UTF8ToUTF16(base::NumberToString(
      calendar_utils::GetExploded(
          calendar_view_controller_->GetOnScreenMonthFirstDay())
          .year)));
  header_year_->SetBorder(
      views::CreateEmptyBorder(0, kLabelTextInBetweenPadding, 0, 0));
  header_year_->SetTextContext(CONTEXT_CALENDAR_LABEL);
  header_year_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);

  TriView* tri_view = TrayPopupUtils::CreateDefaultRowView();
  tri_view->SetBorder(views::CreateEmptyBorder(kLabelVerticalPadding,
                                               kContentHorizontalPadding, 0,
                                               kContentHorizontalPadding));
  tri_view->AddView(TriView::Container::START, header_);
  tri_view->AddView(TriView::Container::START, header_year_);

  auto* down_button = new HeaderButton(
      base::BindRepeating(&CalendarView::ScrollDownOneMonthAndAutoScroll,
                          base::Unretained(this)),
      vector_icons::kCaretDownIcon,
      IDS_ASH_CALENDAR_DOWN_BUTTON_ACCESSIBLE_DESCRIPTION);
  auto* up_button = new HeaderButton(
      base::BindRepeating(&CalendarView::ScrollUpOneMonthAndAutoScroll,
                          base::Unretained(this)),
      vector_icons::kCaretUpIcon,
      IDS_ASH_CALENDAR_UP_BUTTON_ACCESSIBLE_DESCRIPTION);

  tri_view->AddView(TriView::Container::END, down_button);
  tri_view->AddView(TriView::Container::END, up_button);

  AddChildView(tri_view);

  // Add month header.
  auto month_header = std::make_unique<MonthHeaderView>();
  month_header->SetBorder(views::CreateEmptyBorder(
      0, kContentHorizontalPadding, 0, kContentHorizontalPadding));
  AddChildView(std::move(month_header));

  // Add scroll view.
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view_->SetBackgroundColor(absl::nullopt);
  scroll_view_->ClipHeightTo(0, INT_MAX);
  scroll_view_->SetDrawOverflowIndicator(false);
  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);

  content_view_ = scroll_view_->SetContents(std::make_unique<views::View>());
  content_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  content_view_->SetBorder(views::CreateEmptyBorder(kContentInsets));

  SetMonthViews();

  scoped_scroll_view_observer_.Observe(scroll_view_);
  scoped_calendar_view_controller_observer_.Observe(calendar_view_controller_);
  scoped_view_observer_.Observe(scroll_view_);
}

CalendarView::~CalendarView() = default;

void CalendarView::CreateExtraTitleRowButtons() {
  DCHECK(!reset_to_today_button_);
  tri_view()->SetContainerVisible(TriView::Container::END, /*visible=*/true);

  reset_to_today_button_ = CreateInfoButton(
      base::BindRepeating(&CalendarView::ResetToToday, base::Unretained(this)),
      IDS_ASH_CALENDA_INFO_BUTTON);
  tri_view()->AddView(TriView::Container::END, reset_to_today_button_);

  DCHECK(!settings_button_);
  settings_button_ = CreateSettingsButton(
      base::BindRepeating(
          &UnifiedSystemTrayController::HandleOpenDateTimeSettingsAction,
          base::Unretained(controller_)),
      IDS_ASH_CALENDAR_SETTINGS);
  tri_view()->AddView(TriView::Container::END, settings_button_);
}

views::Button* CalendarView::CreateInfoButton(
    views::Button::PressedCallback callback,
    int info_accessible_name_id) {
  return new RoundedLabelButton(
      std::move(callback),
      l10n_util::GetStringUTF16(IDS_ASH_CALENDA_INFO_BUTTON));
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

int CalendarView::PositionOfCurrentMonth() {
  return kContentVerticalPadding +
         previous_label_->GetPreferredSize().height() +
         previous_month_->GetPreferredSize().height() +
         current_label_->GetPreferredSize().height();
}

int CalendarView::PositionOfToday() {
  return PositionOfCurrentMonth() +
         calendar_view_controller_->today_row_top_height();
}

void CalendarView::ResetToToday() {
  calendar_view_controller_->UpdateMonth(base::Time::Now());
  content_view_->RemoveChildViewT(previous_label_);
  content_view_->RemoveChildViewT(previous_month_);
  content_view_->RemoveChildViewT(current_label_);
  content_view_->RemoveChildViewT(current_month_);
  content_view_->RemoveChildViewT(next_label_);
  content_view_->RemoveChildViewT(next_month_);

  // Before adding new label and month views, reset the `scroll_view_` to 0
  // position. Otherwise after all the views are deleted the 'scroll_view_`'s
  // position is still pointing to the position of the previous `current_month_`
  // view which is outside of the view. This will cause the scroll view show
  // nothing on the screen and get stuck there (can not scroll up and down any
  // more).
  {
    base::AutoReset<bool> is_resetting_scrolling(&is_resetting_scroll_, true);
    scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(), 0);
  }

  SetMonthViews();
  ScrollToToday();
}

void CalendarView::ScrollToToday() {
  {
    base::AutoReset<bool> is_resetting_scrolling(&is_resetting_scroll_, true);
    scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                   PositionOfCurrentMonth());
  }

  // If the screen does not have enough height which makes today's cell not in
  // the visible rect, we auto scroll to today's row instead of scrolling to the
  // first row of the current month.
  if (PositionOfCurrentMonth() +
          calendar_view_controller_->today_row_bottom_height() >
      scroll_view_->GetVisibleRect().bottom()) {
    base::AutoReset<bool> is_resetting_scrolling(&is_resetting_scroll_, true);
    scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                   PositionOfToday());
  }
}

void CalendarView::OnThemeChanged() {
  views::View::OnThemeChanged();

  header_->SetEnabledColor(calendar_utils::GetPrimaryTextColor());
  header_year_->SetEnabledColor(calendar_utils::GetSecondaryTextColor());
}

void CalendarView::OnViewBoundsChanged(views::View* observed_view) {
  // Initializes the view to auto scroll to `PositionOfToday` or the first row
  // of today's month. This init needs to be done after the view is drawn
  // (bounds has changed), otherwise we cannot get the bounds of each view.
  // After the first time auto scroll, the view is drawn and we don't need to
  // observe it anymore.
  scoped_view_observer_.Reset();
  ScrollToToday();
}

views::Label* CalendarView::AddLabelWithId(std::u16string label_string,
                                           bool add_at_front) {
  auto label = std::make_unique<CalendarLabel>(label_string);
  label->SetBorder(views::CreateEmptyBorder(kLabelVerticalPadding, 0,
                                            kLabelVerticalPadding, 0));
  label->SetTextContext(CONTEXT_CALENDAR_LABEL);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);
  if (add_at_front)
    return content_view_->AddChildViewAt(std::move(label), 0);
  return content_view_->AddChildView(std::move(label));
}

CalendarMonthView* CalendarView::AddMonth(base::Time month_first_date,
                                          bool add_at_front) {
  auto month = std::make_unique<CalendarMonthView>(month_first_date,
                                                   calendar_view_controller_);
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
  } else if (scroll_view_->GetVisibleRect().y() >= next_label_->y() ||
             scroll_view_->GetVisibleRect().y() +
                     scroll_view_->GetVisibleRect().height() >
                 next_month_->y() + next_month_->height() - kPrepareEndOfView) {
    ScrollDownOneMonth();
  }
}

void CalendarView::OnMonthChanged(const base::Time::Exploded current_month) {
  std::u16string year_string =
      base::UTF8ToUTF16(base::NumberToString(current_month.year));

  header_->SetText(calendar_view_controller_->GetOnScreenMonthName());
  header_year_->SetText(year_string);
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
  previous_label_ =
      AddLabelWithId(calendar_view_controller_->GetPreviousMonthName(),
                     /*add_at_front=*/true);

  // After adding a new month in the content, the current position stays the
  // same but below the added view the each view's position has changed to
  // [original position + new month's height]. So we need to add the height of
  // the newly added month to keep the current view's position.
  int added_height = previous_month_->GetPreferredSize().height() +
                     previous_label_->GetPreferredSize().height();
  int position = added_height + scroll_view_->GetVisibleRect().y();

  base::AutoReset<bool> is_resetting_scrolling(&is_resetting_scroll_, true);
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(), position);
}

void CalendarView::ScrollDownOneMonth() {
  // Renders the next month if the next month label is moving up and passing
  // the top of the visible area, or the next month body's bottom is passing
  // the bottom of the visible area.
  int removed_height = previous_month_->GetPreferredSize().height() +
                       previous_label_->GetPreferredSize().height();

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

  // Same as adding previous views. We need to remove the height of the
  // deleted month to keep the current view's position.
  int position = scroll_view_->GetVisibleRect().y() - removed_height;

  base::AutoReset<bool> is_resetting_scrolling(&is_resetting_scroll_, true);
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(), position);
}

void CalendarView::ScrollUpOneMonthAndAutoScroll() {
  if (is_resetting_scroll_)
    return;

  base::AutoReset<bool> is_resetting_scrolling(&is_resetting_scroll_, true);
  ScrollUpOneMonth();
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                 PositionOfCurrentMonth());
}

void CalendarView::ScrollDownOneMonthAndAutoScroll() {
  if (is_resetting_scroll_)
    return;

  base::AutoReset<bool> is_resetting_scrolling(&is_resetting_scroll_, true);
  ScrollDownOneMonth();
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                 PositionOfCurrentMonth());
}

BEGIN_METADATA(CalendarView, views::View)
END_METADATA

}  // namespace ash
