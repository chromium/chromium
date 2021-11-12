// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_view.h"

#include "ash/public/cpp/ash_typography.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/button_style.h"
#include "ash/system/time/calendar_month_view.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/table_layout.h"
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

// After the user is finished navigating to a different month, this is how long
// we wait before fetchiung more events.
constexpr base::TimeDelta kScrollingSettledTimeout = base::Milliseconds(100);

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
    views::Label::SetAutoColorReadabilityEnabled(false);
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
    views::TableLayout* layout =
        SetLayoutManager(std::make_unique<views::TableLayout>());
    calendar_utils::SetUpWeekColumns(layout);
    layout->AddRows(1, views::TableLayout::kFixedSize);

    for (int week_day : kDefaultWeekTitles) {
      auto label =
          std::make_unique<CalendarLabel>(l10n_util::GetStringUTF16(week_day));
      label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
      label->SetBorder(
          views::CreateEmptyBorder(calendar_utils::kDateCellInsets));
      label->SetElideBehavior(gfx::NO_ELIDE);
      label->SetSubpixelRenderingEnabled(false);
      label->SetTextContext(CONTEXT_CALENDAR_DATE);

      AddChildView(std::move(label));
    }
  }

  MonthHeaderView(const MonthHeaderView& other) = delete;
  MonthHeaderView& operator=(const MonthHeaderView& other) = delete;
  ~MonthHeaderView() override = default;
};

}  // namespace

// The label for each month.
class CalendarView::MonthYearHeaderView : public views::View {
 public:
  MonthYearHeaderView(LabelType type,
                      CalendarViewController* calendar_view_controller)
      : month_label_(AddChildView(std::make_unique<views::Label>())) {
    switch (type) {
      case PREVIOUS:
        date_ = calendar_view_controller->GetPreviousMonthFirstDayLocal(1);
        month_name_ = calendar_view_controller->GetPreviousMonthName();
        break;
      case CURRENT:
        date_ = calendar_view_controller->GetOnScreenMonthFirstDayLocal();
        month_name_ = calendar_view_controller->GetOnScreenMonthName();
        break;
      case NEXT:
        date_ = calendar_view_controller->GetNextMonthFirstDayLocal(1);
        month_name_ = calendar_view_controller->GetNextMonthName();
        break;
    }
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));

    month_label_->SetText(month_name_);
    SetupLabel(month_label_);
    month_label_->SetBorder(
        views::CreateEmptyBorder(gfx::Insets(kLabelVerticalPadding, 0)));

    if (calendar_utils::GetExplodedLocal(date_).year !=
        calendar_utils::GetExplodedLocal(base::Time::Now()).year) {
      year_label_ = AddChildView(std::make_unique<views::Label>());
      year_label_->SetText(base::UTF8ToUTF16(
          base::NumberToString(calendar_utils::GetExplodedLocal(date_).year)));
      SetupLabel(year_label_);
      year_label_->SetBorder(views::CreateEmptyBorder(
          gfx::Insets(kLabelVerticalPadding, kLabelTextInBetweenPadding)));
    }
  }
  MonthYearHeaderView(const MonthYearHeaderView&) = delete;
  MonthYearHeaderView& operator=(const MonthYearHeaderView&) = delete;
  ~MonthYearHeaderView() override = default;

  // views::View:
  void OnThemeChanged() override {
    views::View::OnThemeChanged();

    month_label_->SetEnabledColor(calendar_utils::GetPrimaryTextColor());
    if (year_label_)
      year_label_->SetEnabledColor(calendar_utils::GetSecondaryTextColor());
  }

  void SetupLabel(views::Label* label) {
    label->SetTextContext(CONTEXT_CALENDAR_LABEL);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);
  }

 private:
  // This `date_`'s month and year is used to create this view.
  base::Time date_;

  // The name of the `date_` month.
  std::u16string month_name_;

  // The month label in the view.
  views::Label* const month_label_ = nullptr;

  // The year label in the view.
  views::Label* year_label_ = nullptr;
};

CalendarView::CalendarView(DetailedViewDelegate* delegate,
                           UnifiedSystemTrayController* controller)
    : TrayDetailedView(delegate),
      controller_(controller),
      calendar_view_controller_(std::make_unique<CalendarViewController>()),
      scrolling_settled_timer_(
          FROM_HERE,
          kScrollingSettledTimeout,
          base::BindRepeating(&CalendarView::OnScrollingSettledTimerFired,
                              base::Unretained(this))) {
  CreateTitleRow(IDS_ASH_CALENDAR_TITLE);

  // Add the header.
  header_ = TrayPopupUtils::CreateDefaultLabel();
  header_->SetText(calendar_view_controller_->GetOnScreenMonthName());
  header_->SetTextContext(CONTEXT_CALENDAR_LABEL);
  header_->SetAutoColorReadabilityEnabled(false);
  header_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);

  header_year_ = TrayPopupUtils::CreateDefaultLabel();
  header_year_->SetText(base::UTF8ToUTF16(base::NumberToString(
      calendar_utils::GetExplodedLocal(
          calendar_view_controller_->GetOnScreenMonthFirstDayLocal())
          .year)));
  header_year_->SetBorder(views::CreateEmptyBorder(
      0, kLabelTextInBetweenPadding, 0, kLabelTextInBetweenPadding));
  header_year_->SetTextContext(CONTEXT_CALENDAR_LABEL);
  header_->SetAutoColorReadabilityEnabled(false);
  header_year_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);

  TriView* tri_view = TrayPopupUtils::CreateDefaultRowView();
  tri_view->SetBorder(views::CreateEmptyBorder(kLabelVerticalPadding,
                                               kContentHorizontalPadding, 0,
                                               kContentHorizontalPadding));
  tri_view->AddView(TriView::Container::START, header_);
  tri_view->AddView(TriView::Container::START, header_year_);

  down_button_ = new HeaderButton(
      base::BindRepeating(&CalendarView::ScrollDownOneMonthAndAutoScroll,
                          base::Unretained(this)),
      vector_icons::kCaretDownIcon,
      IDS_ASH_CALENDAR_DOWN_BUTTON_ACCESSIBLE_DESCRIPTION);
  up_button_ = new HeaderButton(
      base::BindRepeating(&CalendarView::ScrollUpOneMonthAndAutoScroll,
                          base::Unretained(this)),
      vector_icons::kCaretUpIcon,
      IDS_ASH_CALENDAR_UP_BUTTON_ACCESSIBLE_DESCRIPTION);

  tri_view->AddView(TriView::Container::END, down_button_);
  tri_view->AddView(TriView::Container::END, up_button_);

  AddChildView(tri_view);

  // Add month header.
  auto month_header = std::make_unique<MonthHeaderView>();
  month_header->SetBorder(views::CreateEmptyBorder(
      0, kContentHorizontalPadding, 0, kContentHorizontalPadding));
  AddChildView(std::move(month_header));

  // Add scroll view.
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view_->SetAllowKeyboardScrolling(false);
  scroll_view_->SetBackgroundColor(absl::nullopt);
  scroll_view_->ClipHeightTo(0, INT_MAX);
  scroll_view_->SetDrawOverflowIndicator(false);
  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  on_contents_scrolled_subscription_ =
      scroll_view_->AddContentsScrolledCallback(base::BindRepeating(
          &CalendarView::OnContentsScrolled, base::Unretained(this)));

  content_view_ = scroll_view_->SetContents(std::make_unique<views::View>());
  content_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  content_view_->SetBorder(views::CreateEmptyBorder(kContentInsets));
  // Focusable nodes must have an accessible name.
  content_view_->GetViewAccessibility().OverrideName(GetClassName());
  content_view_->SetFocusBehavior(FocusBehavior::ALWAYS);

  SetMonthViews();

  scoped_calendar_view_controller_observer_.Observe(
      calendar_view_controller_.get());
  scoped_view_observer_.AddObservation(scroll_view_);
  scoped_view_observer_.AddObservation(content_view_);
}

CalendarView::~CalendarView() = default;

void CalendarView::Init() {
  calendar_view_controller_->FetchEvents();
}

void CalendarView::CreateExtraTitleRowButtons() {
  DCHECK(!reset_to_today_button_);
  tri_view()->SetContainerVisible(TriView::Container::END, /*visible=*/true);

  reset_to_today_button_ = CreateInfoButton(
      base::BindRepeating(&CalendarView::ResetToToday, base::Unretained(this)),
      IDS_ASH_CALENDAR_INFO_BUTTON_ACCESSIBLE_DESCRIPTION);
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
  auto* button =
      new PillButton(std::move(callback),
                     l10n_util::GetStringUTF16(IDS_ASH_CALENDA_INFO_BUTTON),
                     PillButton::Type::kIconless, /*icon=*/nullptr);
  button->SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_ASH_CALENDAR_INFO_BUTTON_ACCESSIBLE_DESCRIPTION,
      base::TimeFormatWithPattern(base::Time::Now(), "MMMMdyyyy")));
  return button;
}

void CalendarView::SetMonthViews() {
  previous_label_ = AddLabelWithId(LabelType::PREVIOUS);
  previous_month_ =
      AddMonth(calendar_view_controller_->GetPreviousMonthFirstDayLocal(1));

  current_label_ = AddLabelWithId(LabelType::CURRENT);
  current_month_ =
      AddMonth(calendar_view_controller_->GetOnScreenMonthFirstDayLocal());

  next_label_ = AddLabelWithId(LabelType::NEXT);
  next_month_ =
      AddMonth(calendar_view_controller_->GetNextMonthFirstDayLocal(1));
}

int CalendarView::PositionOfCurrentMonth() {
  return kContentVerticalPadding +
         previous_label_->GetPreferredSize().height() +
         previous_month_->GetPreferredSize().height() +
         current_label_->GetPreferredSize().height();
}

int CalendarView::PositionOfToday() {
  return PositionOfCurrentMonth() +
         calendar_view_controller_->GetTodayRowTopHeight();
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
  MaybeResetContentViewFocusBehavior();
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
          calendar_view_controller_->GetTodayRowBottomHeight() >
      scroll_view_->GetVisibleRect().bottom()) {
    base::AutoReset<bool> is_resetting_scrolling(&is_resetting_scroll_, true);
    scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                   PositionOfToday());
  }
}

bool CalendarView::IsDateCellViewFocused() {
  // For tests, in which the view is not in a Widget.
  if (!GetFocusManager())
    return false;

  auto* focused_view = GetFocusManager()->GetFocusedView();
  if (!focused_view)
    return false;

  return focused_view->GetClassName() == CalendarDateCellView::kViewClassName;
}

void CalendarView::MaybeResetContentViewFocusBehavior() {
  if (IsDateCellViewFocused() ||
      content_view_->GetFocusBehavior() == FocusBehavior::ALWAYS) {
    return;
  }

  content_view_->SetFocusBehavior(FocusBehavior::ALWAYS);
}

void CalendarView::OnThemeChanged() {
  views::View::OnThemeChanged();

  header_->SetEnabledColor(calendar_utils::GetPrimaryTextColor());
  header_year_->SetEnabledColor(calendar_utils::GetSecondaryTextColor());
}

void CalendarView::OnViewBoundsChanged(views::View* observed_view) {
  if (observed_view != scroll_view_)
    return;

  // Initializes the view to auto scroll to `PositionOfToday` or the first row
  // of today's month. This init needs to be done after the view is drawn
  // (bounds has changed), otherwise we cannot get the bounds of each view.
  // After the first time auto scroll, the view is drawn and we don't need to
  // observe it anymore.
  scoped_view_observer_.RemoveObservation(observed_view);
  ScrollToToday();
}

void CalendarView::OnViewFocused(View* observed_view) {
  if (observed_view != content_view_ || IsDateCellViewFocused())
    return;

  // When focusing on the `content_view_`, we decide which is the to-be-focued
  // cell based on the current position.
  previous_month_->EnableFocus();
  current_month_->EnableFocus();
  next_month_->EnableFocus();
  auto* focus_manager = GetFocusManager();
  const int position = scroll_view_->GetVisibleRect().y();
  const int row_height = calendar_view_controller_->row_height();

  // At least one row of the current month is visible on the screen. The
  // to-be-focused cell should be the first non-grayed date cell that is
  // visible, or today's cell if today is in the current month and visible.
  if (position < (next_label_->y() - row_height)) {
    int row_index = 0;
    const int today_index = calendar_view_controller_->today_row() - 1;
    while (position > (PositionOfCurrentMonth() + row_index * row_height))
      ++row_index;

    if (current_month_->has_today() && row_index <= today_index) {
      focus_manager->SetFocusedView(
          current_month_->focused_cells()[today_index]);
    } else {
      focus_manager->SetFocusedView(current_month_->focused_cells()[row_index]);
    }
  } else {
    // If there's no visible row of the current month on the screen, focus on
    // the first visible non-grayed-out date of the next month.
    focus_manager->SetFocusedView(next_month_->focused_cells().front());
  }
  content_view_->SetFocusBehavior(FocusBehavior::NEVER);
}

views::View* CalendarView::AddLabelWithId(LabelType type, bool add_at_front) {
  auto label = std::make_unique<MonthYearHeaderView>(
      type, calendar_view_controller_.get());
  if (add_at_front)
    return content_view_->AddChildViewAt(std::move(label), 0);
  return content_view_->AddChildView(std::move(label));
}

CalendarMonthView* CalendarView::AddMonth(base::Time month_first_date,
                                          bool add_at_front) {
  auto month = std::make_unique<CalendarMonthView>(
      month_first_date, calendar_view_controller_.get());
  month->SetBorder(views::CreateEmptyBorder(kMonthVerticalPadding, 0,
                                            kMonthVerticalPadding, 0));
  if (add_at_front) {
    return content_view_->AddChildViewAt(std::move(month), 0);
  } else {
    return content_view_->AddChildView(std::move(month));
  }
}

void CalendarView::OnMonthChanged(const base::Time::Exploded current_month) {
  std::u16string year_string =
      base::UTF8ToUTF16(base::NumberToString(current_month.year));

  header_->SetText(calendar_view_controller_->GetOnScreenMonthName());
  header_year_->SetText(year_string);

  scrolling_settled_timer_.Reset();
}

void CalendarView::OnEventsFetched(
    const google_apis::calendar::EventList* events) {
  // No need to store the events, but we need to notify the month views that
  // something may have changed and they need to refresh.
  SchedulePaint();
}

void CalendarView::ScrollUpOneMonth() {
  calendar_view_controller_->UpdateMonth(
      calendar_view_controller_->GetPreviousMonthFirstDayLocal(1));
  content_view_->RemoveChildViewT(next_label_);
  content_view_->RemoveChildViewT(next_month_);

  next_label_ = current_label_;
  next_month_ = current_month_;
  current_label_ = previous_label_;
  current_month_ = previous_month_;

  previous_month_ =
      AddMonth(calendar_view_controller_->GetPreviousMonthFirstDayLocal(1),
               /*add_at_front=*/true);
  if (IsDateCellViewFocused())
    previous_month_->EnableFocus();
  previous_label_ = AddLabelWithId(LabelType::PREVIOUS,
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

  MaybeResetContentViewFocusBehavior();
}

void CalendarView::ScrollDownOneMonth() {
  // Renders the next month if the next month label is moving up and passing
  // the top of the visible area, or the next month body's bottom is passing
  // the bottom of the visible area.
  int removed_height = previous_month_->GetPreferredSize().height() +
                       previous_label_->GetPreferredSize().height();

  calendar_view_controller_->UpdateMonth(
      calendar_view_controller_->GetNextMonthFirstDayLocal(1));

  content_view_->RemoveChildViewT(previous_label_);
  content_view_->RemoveChildViewT(previous_month_);

  previous_label_ = current_label_;
  previous_month_ = current_month_;
  current_label_ = next_label_;
  current_month_ = next_month_;

  next_label_ = AddLabelWithId(LabelType::NEXT);
  next_month_ =
      AddMonth(calendar_view_controller_->GetNextMonthFirstDayLocal(1));
  if (IsDateCellViewFocused())
    next_month_->EnableFocus();

  // Same as adding previous views. We need to remove the height of the
  // deleted month to keep the current view's position.
  int position = scroll_view_->GetVisibleRect().y() - removed_height;

  base::AutoReset<bool> is_resetting_scrolling(&is_resetting_scroll_, true);
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(), position);

  MaybeResetContentViewFocusBehavior();
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

void CalendarView::OnEvent(ui::Event* event) {
  if (!event->IsKeyEvent() || !IsDateCellViewFocused()) {
    TrayDetailedView::OnEvent(event);
    return;
  }

  auto* key_event = event->AsKeyEvent();
  auto key_code = key_event->key_code();
  auto* focus_manager = GetFocusManager();

  // When tab key is pressed, stops focusing on any `CalendarDateCellView` and
  // goes to the next focusable button in the header.
  if (key_event->type() == ui::EventType::ET_KEY_PRESSED &&
      views::FocusManager::IsTabTraversalKeyEvent(*key_event)) {
    // Set focus on null pointer first. Otherwise the it will auto
    // `AdvanceFocus` when the focused cell is on blur.
    focus_manager->SetFocusedView(nullptr);
    current_month_->DisableFocus();
    previous_month_->DisableFocus();
    next_month_->DisableFocus();
    TrayDetailedView::OnEvent(event);
    content_view_->SetFocusBehavior(FocusBehavior::ALWAYS);
    return;
  }

  if (key_event->type() != ui::EventType::ET_KEY_PRESSED ||
      (key_code != ui::VKEY_UP && key_code != ui::VKEY_DOWN &&
       key_code != ui::VKEY_LEFT && key_code != ui::VKEY_RIGHT)) {
    TrayDetailedView::OnEvent(event);
    return;
  }

  switch (key_code) {
    case ui::VKEY_UP:
    case ui::VKEY_DOWN: {
      auto* current_focusable_view = focus_manager->GetFocusedView();

      // Moving 7 (`kDateInOneWeek`) steps will focus on the cell which is right
      // above or below the current cell, since each row has 7 days.
      for (int count = 0; count < calendar_utils::kDateInOneWeek; count++) {
        auto* next_focusable_view = focus_manager->GetNextFocusableView(
            current_focusable_view, GetWidget(),
            /*reverse=*/key_code == ui::VKEY_UP,
            /*dont_loop=*/false);

        // There could be a corner case that the next month view is very short
        // (e.g. February in some year), and except the last 2 rows all the
        // other rows of it are visible on the screen. In this case if the
        // current focused view is in the second to last row of this next month,
        // the next to-be-focused cell could be in the first row of the next
        // month's next month. But at this time the next month's next month is
        // not created yet since it did not trigger the condition (which is
        // either the next month's label hit the top of the scroll window or the
        // last row of the next month hit the bottom of the scroll window) to
        // build it. Now since it cannot find the next next month, it will focus
        // on the `previous_month_`'s first focusable cell
        // (`next_focusable_view->y() < current_focusable_view->y()`). So here
        // if we get to this corner case, we manually scroll down 2 rows to make
        // sure the next next month get created when needed.
        if (key_code == ui::VKEY_DOWN && next_focusable_view &&
            current_focusable_view->GetClassName() ==
                CalendarDateCellView::kViewClassName &&
            next_focusable_view->y() < current_focusable_view->y()) {
          // Scrolls down 2 rows.
          scroll_view_->ScrollToPosition(
              scroll_view_->vertical_scroll_bar(),
              scroll_view_->GetVisibleRect().y() +
                  2 * calendar_view_controller_->row_height());
          next_focusable_view = focus_manager->GetNextFocusableView(
              current_focusable_view, GetWidget(),
              /*reverse=*/key_code == ui::VKEY_UP,
              /*dont_loop=*/false);
        }
        current_focusable_view = next_focusable_view;
        // Sometimes the position of the upper row cells, which should be
        // focused next, are above (and hidden behind) the header buttons. So
        // this loop skips those buttons.
        while (current_focusable_view &&
               current_focusable_view->GetClassName() !=
                   CalendarDateCellView::kViewClassName) {
          current_focusable_view = focus_manager->GetNextFocusableView(
              current_focusable_view, GetWidget(),
              /*reverse=*/key_code == ui::VKEY_UP,
              /*dont_loop=*/false);
        }
      }
      focus_manager->SetFocusedView(current_focusable_view);
      return;
    }
    case ui::VKEY_LEFT:
    case ui::VKEY_RIGHT: {
      bool is_reverse = base::i18n::IsRTL() ? key_code == ui::VKEY_RIGHT
                                            : key_code == ui::VKEY_LEFT;
      focus_manager->AdvanceFocus(/*reverse=*/is_reverse);
      return;
    }
    default:
      NOTREACHED();
  }
}

void CalendarView::OnScrollingSettledTimerFired() {
  calendar_view_controller_->FetchEvents();
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

BEGIN_METADATA(CalendarView, views::View)
END_METADATA

}  // namespace ash
