// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_view.h"
#include <memory>

#include "ash/public/cpp/ash_typography.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_event_list_view.h"
#include "ash/system/time/calendar_metrics.h"
#include "ash/system/time/calendar_month_view.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/calendar_view_controller.h"
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
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
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
constexpr int kWeekRowHorizontalPadding =
    kContentHorizontalPadding - calendar_utils::kDateHorizontalPadding;

// The percentage of a normal row height, which (percentage * row_height) will
// be used as the `CalendarView` height when the `CalendarEventListView` is
// expanded.
constexpr float kExpandedCalendarViewHeightScale = 1.2;

// After the user is finished navigating to a different month, this is how long
// we wait before fetchiung more events.
constexpr base::TimeDelta kScrollingSettledTimeout = base::Milliseconds(100);

// Duration of the delay for modifying opacity.
constexpr base::TimeDelta kDelayVisibilityAnimationDuration =
    base::Milliseconds(200);

// Duration of events moving animation.
constexpr base::TimeDelta kAnimationDurationForEventsMoving =
    base::Milliseconds(400);

// Duration of closing events panel animation.
constexpr base::TimeDelta kAnimationDurationForClosingEvents =
    base::Milliseconds(200);

// The cool-down time for enabling animation.
constexpr base::TimeDelta kAnimationDisablingTimeout = base::Milliseconds(500);

// TODO(https://crbug.com/1236276): for some language it may start from "M".
constexpr int kDefaultWeekTitles[] = {
    IDS_ASH_CALENDAR_SUN, IDS_ASH_CALENDAR_MON, IDS_ASH_CALENDAR_TUE,
    IDS_ASH_CALENDAR_WED, IDS_ASH_CALENDAR_THU, IDS_ASH_CALENDAR_FRI,
    IDS_ASH_CALENDAR_SAT};

constexpr char kMonthViewScrollOneMonthAnimationHistogram[] =
    "Ash.CalendarView.ScrollOneMonth.MonthView.AnimationSmoothness";

constexpr char kLabelViewScrollOneMonthAnimationHistogram[] =
    "Ash.CalendarView.ScrollOneMonth.LabelView.AnimationSmoothness";

constexpr char kHeaderViewScrollOneMonthAnimationHistogram[] =
    "Ash.CalendarView.ScrollOneMonth.HeaderView.AnimationSmoothness";

constexpr char kContentViewResetToTodayAnimationHistogram[] =
    "Ash.CalendarView.ResetToToday.ContentView.AnimationSmoothness";

constexpr char kHeaderViewResetToTodayAnimationHistogram[] =
    "Ash.CalendarView.ResetToToday.HeaderView.AnimationSmoothness";

constexpr char kContentViewFadeInCurrentMonthAnimationHistogram[] =
    "Ash.CalendarView.FadeInCurrentMonth.ContentView.AnimationSmoothness";

constexpr char kHeaderViewFadeInCurrentMonthAnimationHistogram[] =
    "Ash.CalendarView.FadeInCurrentMonth.HeaderView.AnimationSmoothness";

constexpr char kOnMonthChangedAnimationHistogram[] =
    "Ash.CalendarView.OnMonthChanged.AnimationSmoothness";

constexpr char kCloseEventListAnimationHistogram[] =
    "Ash.CalendarView.CloseEventList.AnimationSmoothness";

constexpr char kMonthViewOpenEventListAnimationHistogram[] =
    "Ash.CalendarView.OpenEventList.MonthView.AnimationSmoothness";

constexpr char kLabelViewOpenEventListAnimationHistogram[] =
    "Ash.CalendarView.OpenEventList.LabelView.AnimationSmoothness";

constexpr char kEventListViewOpenEventListAnimationHistogram[] =
    "Ash.CalendarView.OpenEventList.EventListView.AnimationSmoothness";

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
      label->SetBorder((views::CreateEmptyBorder(
          gfx::Insets(calendar_utils::kDateVerticalPadding, 0))));
      label->SetElideBehavior(gfx::NO_ELIDE);
      label->SetSubpixelRenderingEnabled(false);
      label->SetFontList(
          views::style::GetFont(CONTEXT_CALENDAR_DATE, STYLE_EMPHASIZED));

      AddChildView(std::move(label));
    }
  }

  MonthHeaderView(const MonthHeaderView& other) = delete;
  MonthHeaderView& operator=(const MonthHeaderView& other) = delete;
  ~MonthHeaderView() override = default;
};

// Resets the `view`'s opacity and position.
void ResetLayer(views::View* view) {
  view->layer()->SetOpacity(1.0f);
  view->layer()->SetTransform(gfx::Transform());
}

}  // namespace

// The label for each month.
class CalendarView::MonthHeaderLabelView : public views::View {
 public:
  MonthHeaderLabelView(LabelType type,
                       CalendarViewController* calendar_view_controller)
      : month_label_(AddChildView(std::make_unique<views::Label>())) {
    // The layer is required in animation.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
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
      case NEXTNEXT:
        date_ = calendar_view_controller->GetNextMonthFirstDayLocal(2);
        month_name_ =
            calendar_view_controller->GetNextMonthName(/*num_months=*/2);
        break;
    }
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));

    month_label_->SetText(month_name_);
    SetupLabel(month_label_);
    month_label_->SetBorder(views::CreateEmptyBorder(kLabelVerticalPadding,
                                                     kContentHorizontalPadding,
                                                     kLabelVerticalPadding, 0));
  }
  MonthHeaderLabelView(const MonthHeaderLabelView&) = delete;
  MonthHeaderLabelView& operator=(const MonthHeaderLabelView&) = delete;
  ~MonthHeaderLabelView() override = default;

  // views::View:
  void OnThemeChanged() override {
    views::View::OnThemeChanged();

    month_label_->SetEnabledColor(calendar_utils::GetPrimaryTextColor());
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
};

CalendarView::ScrollContentsView::ScrollContentsView(
    CalendarViewController* controller)
    : controller_(controller),
      stylus_event_handler_(this),
      current_month_(controller_->GetOnScreenMonthName()) {}

void CalendarView::ScrollContentsView::OnMonthChanged() {
  current_month_ = controller_->GetOnScreenMonthName();
}

void CalendarView::ScrollContentsView::OnEvent(ui::Event* event) {
  views::View::OnEvent(event);

  if (controller_->GetOnScreenMonthName() == current_month_)
    return;
  OnMonthChanged();

  if (event->IsMouseWheelEvent()) {
    calendar_metrics::RecordScrollSource(
        calendar_metrics::CalendarViewScrollSource::kByMouseWheel);
  }

  if (event->IsScrollGestureEvent()) {
    calendar_metrics::RecordScrollSource(
        calendar_metrics::CalendarViewScrollSource::kByGesture);
  }

  if (event->IsFlingScrollEvent()) {
    calendar_metrics::RecordScrollSource(
        calendar_metrics::CalendarViewScrollSource::kByFling);
  }
}

void CalendarView::ScrollContentsView::OnStylusEvent(
    const ui::TouchEvent& event) {
  if (controller_->GetOnScreenMonthName() == current_month_)
    return;
  OnMonthChanged();

  calendar_metrics::RecordScrollSource(
      calendar_metrics::CalendarViewScrollSource::kByStylus);
}

CalendarView::ScrollContentsView::StylusEventHandler::StylusEventHandler(
    ScrollContentsView* content_view)
    : content_view_(content_view) {
  Shell::Get()->AddPreTargetHandler(this);
}

CalendarView::ScrollContentsView::StylusEventHandler::~StylusEventHandler() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void CalendarView::ScrollContentsView::StylusEventHandler::OnTouchEvent(
    ui::TouchEvent* event) {
  if (event->pointer_details().pointer_type == ui::EventPointerType::kPen) {
    content_view_->OnStylusEvent(*event);
  }
}

CalendarHeaderView::CalendarHeaderView(const std::u16string& month,
                                       const std::u16string& year)
    : header_(AddChildView(std::make_unique<views::Label>())),
      header_year_(AddChildView(std::make_unique<views::Label>())) {
  // The layer is required in animation.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  header_->SetText(month);
  header_->SetTextContext(CONTEXT_CALENDAR_LABEL);
  header_->SetAutoColorReadabilityEnabled(false);
  header_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);

  header_year_->SetText(year);
  header_year_->SetBorder(views::CreateEmptyBorder(
      0, kLabelTextInBetweenPadding, 0, kLabelTextInBetweenPadding));
  header_year_->SetTextContext(CONTEXT_CALENDAR_LABEL);
  header_year_->SetAutoColorReadabilityEnabled(false);
  header_year_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);
}

CalendarHeaderView::~CalendarHeaderView() = default;

void CalendarHeaderView::OnThemeChanged() {
  views::View::OnThemeChanged();

  header_->SetEnabledColor(calendar_utils::GetPrimaryTextColor());
  header_year_->SetEnabledColor(calendar_utils::GetSecondaryTextColor());
}

void CalendarHeaderView::UpdateHeaders(const std::u16string& month,
                                       const std::u16string& year) {
  header_->SetText(month);
  header_year_->SetText(year);
}

CalendarView::CalendarView(DetailedViewDelegate* delegate,
                           UnifiedSystemTrayController* controller)
    : TrayDetailedView(delegate),
      controller_(controller),
      calendar_view_controller_(std::make_unique<CalendarViewController>()),
      scrolling_settled_timer_(
          FROM_HERE,
          kScrollingSettledTimeout,
          base::BindRepeating(&CalendarView::OnScrollingSettledTimerFired,
                              base::Unretained(this))),
      header_animation_restart_timer_(
          FROM_HERE,
          kAnimationDisablingTimeout,
          base::BindRepeating(
              [](CalendarView* calendar_view) {
                if (!calendar_view)
                  return;
                calendar_view->set_should_header_animate(true);
              },
              base::Unretained(this))),
      months_animation_restart_timer_(
          FROM_HERE,
          kAnimationDisablingTimeout,
          base::BindRepeating(
              [](CalendarView* calendar_view) {
                if (!calendar_view)
                  return;
                calendar_view->set_should_months_animate(true);
              },
              base::Unretained(this))) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  GetViewAccessibility().OverrideName(GetClassName());

  CreateTitleRow(IDS_ASH_CALENDAR_TITLE);

  // Add the header.
  header_ = new CalendarHeaderView(
      calendar_view_controller_->GetOnScreenMonthName(),
      base::TimeFormatWithPattern(
          calendar_view_controller_->currently_shown_date(), "YYYY"));

  TriView* tri_view = TrayPopupUtils::CreateDefaultRowView();
  tri_view->SetBorder(views::CreateEmptyBorder(
      kLabelVerticalPadding, kContentHorizontalPadding, 0,
      kContentHorizontalPadding - calendar_utils::kColumnSetPadding));
  tri_view->AddView(TriView::Container::START, header_);

  auto* button_container = new views::View();
  views::BoxLayout* button_container_layout =
      button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  button_container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  // Aligns button with the calendar dates in the `TableLayout`.
  button_container_layout->set_between_child_spacing(
      calendar_utils::kDateHorizontalPadding +
      calendar_utils::kColumnSetPadding);

  down_button_ = button_container->AddChildView(std::make_unique<IconButton>(
      base::BindRepeating(&CalendarView::OnMonthArrowButtonActivated,
                          base::Unretained(this), /*up=*/false),
      IconButton::Type::kSmallFloating, &vector_icons::kCaretDownIcon,
      IDS_ASH_CALENDAR_DOWN_BUTTON_ACCESSIBLE_DESCRIPTION));

  up_button_ = button_container->AddChildView(std::make_unique<IconButton>(
      base::BindRepeating(&CalendarView::OnMonthArrowButtonActivated,
                          base::Unretained(this), /*up=*/true),
      IconButton::Type::kSmallFloating, &vector_icons::kCaretUpIcon,
      IDS_ASH_CALENDAR_UP_BUTTON_ACCESSIBLE_DESCRIPTION));

  tri_view->AddView(TriView::Container::END, button_container);
  AddChildView(tri_view);

  // Add month header.
  auto month_header = std::make_unique<MonthHeaderView>();
  month_header->SetBorder(views::CreateEmptyBorder(
      0, kWeekRowHorizontalPadding, 0, kWeekRowHorizontalPadding));
  AddChildView(std::move(month_header));

  // Add scroll view.
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view_->SetAllowKeyboardScrolling(false);
  scroll_view_->SetBackgroundColor(absl::nullopt);
  scroll_view_->ClipHeightTo(0, INT_MAX);
  scroll_view_->SetDrawOverflowIndicator(false);
  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  scroll_view_->SetFocusBehavior(FocusBehavior::NEVER);
  on_contents_scrolled_subscription_ =
      scroll_view_->AddContentsScrolledCallback(base::BindRepeating(
          &CalendarView::OnContentsScrolled, base::Unretained(this)));

  content_view_ = scroll_view_->SetContents(
      std::make_unique<ScrollContentsView>(calendar_view_controller_.get()));
  content_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  content_view_->SetBorder(views::CreateEmptyBorder(
      kContentVerticalPadding, kWeekRowHorizontalPadding,
      kContentVerticalPadding, kWeekRowHorizontalPadding));
  // Focusable nodes must have an accessible name.
  content_view_->GetViewAccessibility().OverrideName(GetClassName());
  content_view_->SetFocusBehavior(FocusBehavior::ALWAYS);

  SetMonthViews();

  scoped_calendar_model_observer_.Observe(
      Shell::Get()->system_tray_model()->calendar_model());
  scoped_calendar_view_controller_observer_.Observe(
      calendar_view_controller_.get());
  scoped_view_observer_.AddObservation(scroll_view_);
  scoped_view_observer_.AddObservation(content_view_);
  scoped_view_observer_.AddObservation(this);
}

CalendarView::~CalendarView() {
  // Removes child views including month views and event list to remove their
  // dependency from `CalendarViewController`, since these views are destructed
  // after the controller.
  content_view_->RemoveAllChildViews();
  if (event_list_view_) {
    RemoveChildViewT(event_list_view_);
    event_list_view_ = nullptr;
  }
}

void CalendarView::Init() {
  calendar_view_controller_->FetchEvents();
}

void CalendarView::CreateExtraTitleRowButtons() {
  DCHECK(!reset_to_today_button_);
  tri_view()->SetContainerVisible(TriView::Container::END, /*visible=*/true);

  reset_to_today_button_ = CreateInfoButton(
      base::BindRepeating(&CalendarView::ResetToTodayWithAnimation,
                          base::Unretained(this)),
      IDS_ASH_CALENDAR_INFO_BUTTON_ACCESSIBLE_DESCRIPTION);
  reset_to_today_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_TODAY_BUTTON_TOOLTIP));
  tri_view()->AddView(TriView::Container::END, reset_to_today_button_);

  DCHECK(!settings_button_);
  settings_button_ = CreateSettingsButton(
      base::BindRepeating(
          &UnifiedSystemTrayController::HandleOpenDateTimeSettingsAction,
          base::Unretained(controller_)),
      IDS_ASH_CALENDAR_SETTINGS);
  settings_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_SETTINGS_TOOLTIP));
  tri_view()->AddView(TriView::Container::END, settings_button_);
}

views::Button* CalendarView::CreateInfoButton(
    views::Button::PressedCallback callback,
    int info_accessible_name_id) {
  auto* button =
      new PillButton(std::move(callback),
                     l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_INFO_BUTTON),
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

  next_next_label_ = AddLabelWithId(LabelType::NEXTNEXT);
  next_next_month_ = AddMonth(
      calendar_view_controller_->GetNextMonthFirstDayLocal(/*num_months=*/2));
}

int CalendarView::PositionOfCurrentMonth() const {
  return kContentVerticalPadding +
         previous_label_->GetPreferredSize().height() +
         previous_month_->GetPreferredSize().height() +
         current_label_->GetPreferredSize().height();
}

int CalendarView::PositionOfToday() const {
  return PositionOfCurrentMonth() +
         calendar_view_controller_->GetTodayRowTopHeight();
}

int CalendarView::PositionOfSelectedDate() const {
  DCHECK(calendar_view_controller_->selected_date().has_value());
  const int row_height = calendar_view_controller_->selected_date_row_index() *
                             calendar_view_controller_->row_height() +
                         calendar_utils::kDateVerticalPadding;
  // The selected date should be either in the current month or the next month.
  if (calendar_view_controller_->IsSelectedDateInCurrentMonth())
    return PositionOfCurrentMonth() + row_height;

  return PositionOfCurrentMonth() +
         current_month_->GetPreferredSize().height() +
         next_label_->GetPreferredSize().height() + row_height;
}

void CalendarView::SetHeaderAndContentViewOpacity(float opacity) {
  header_->layer()->SetOpacity(opacity);
  content_view_->layer()->SetOpacity(opacity);
}

void CalendarView::SetShouldMonthsAnimateAndScrollEnabled(bool enabled) {
  set_should_months_animate(enabled);
  is_resetting_scroll_ = !enabled;
  scroll_view_->SetVerticalScrollBarMode(
      enabled ? views::ScrollView::ScrollBarMode::kHiddenButEnabled
              : views::ScrollView::ScrollBarMode::kDisabled);
}

void CalendarView::ResetToTodayWithAnimation() {
  if (!should_months_animate_)
    return;
  SetShouldMonthsAnimateAndScrollEnabled(/*enabled=*/false);

  content_view_->SetPaintToLayer();
  content_view_->layer()->SetFillsBoundsOpaquely(false);

  auto content_reporter = calendar_metrics::CreateAnimationReporter(
      content_view_, kContentViewResetToTodayAnimationHistogram);
  auto header_reporter = calendar_metrics::CreateAnimationReporter(
      header_, kHeaderViewResetToTodayAnimationHistogram);

  // Fades out on-screen month. When animation ends sets date to today by
  // calling `ResetToToday` and fades in updated views after.
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(
          [](base::WeakPtr<CalendarView> calendar_view) {
            if (!calendar_view)
              return;
            calendar_view->SetShouldMonthsAnimateAndScrollEnabled(
                /*enabled=*/true);
            calendar_view->ResetToToday();
            calendar_view->FadeInCurrentMonth();
          },
          weak_factory_.GetWeakPtr()))
      .OnAborted(base::BindOnce(
          [](base::WeakPtr<CalendarView> calendar_view) {
            if (!calendar_view)
              return;
            calendar_view->SetShouldMonthsAnimateAndScrollEnabled(
                /*enabled=*/true);
            calendar_view->ResetToToday();
            calendar_view->FadeInCurrentMonth();
          },
          weak_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(calendar_utils::kResetToTodayFadeAnimationDuration)
      .SetOpacity(header_, 0.0f)
      .SetOpacity(content_view_, 0.0f);
}

void CalendarView::ResetToToday() {
  if (event_list_view_) {
    scroll_view_->SetVerticalScrollBarMode(
        views::ScrollView::ScrollBarMode::kHiddenButEnabled);
    set_should_months_animate(false);
  }

  // Updates month to today's date without animating header.
  {
    base::AutoReset<bool> is_updating_month(&should_header_animate_, false);
    calendar_view_controller_->UpdateMonth(base::Time::Now());
  }

  content_view_->RemoveChildViewT(previous_label_);
  content_view_->RemoveChildViewT(previous_month_);
  content_view_->RemoveChildViewT(current_label_);
  content_view_->RemoveChildViewT(current_month_);
  content_view_->RemoveChildViewT(next_label_);
  content_view_->RemoveChildViewT(next_month_);
  content_view_->RemoveChildViewT(next_next_label_);
  content_view_->RemoveChildViewT(next_next_month_);

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

  if (event_list_view_) {
    scroll_view_->SetVerticalScrollBarMode(
        views::ScrollView::ScrollBarMode::kDisabled);
    months_animation_restart_timer_.Reset();
  }
}

void CalendarView::FadeInCurrentMonth() {
  if (!should_months_animate_)
    return;
  SetShouldMonthsAnimateAndScrollEnabled(/*enabled=*/false);

  content_view_->SetPaintToLayer();
  content_view_->layer()->SetFillsBoundsOpaquely(false);
  SetHeaderAndContentViewOpacity(/*opacity=*/0.0f);

  auto content_reporter = calendar_metrics::CreateAnimationReporter(
      content_view_, kContentViewFadeInCurrentMonthAnimationHistogram);
  auto header_reporter = calendar_metrics::CreateAnimationReporter(
      header_, kHeaderViewFadeInCurrentMonthAnimationHistogram);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(
          [](base::WeakPtr<CalendarView> calendar_view) {
            if (!calendar_view)
              return;
            calendar_view->SetShouldMonthsAnimateAndScrollEnabled(
                /*enabled=*/true);
          },
          weak_factory_.GetWeakPtr()))
      .OnAborted(base::BindOnce(
          [](base::WeakPtr<CalendarView> calendar_view) {
            if (!calendar_view)
              return;
            calendar_view->SetShouldMonthsAnimateAndScrollEnabled(
                /*enabled=*/true);
            calendar_view->SetHeaderAndContentViewOpacity(/*opacity=*/1.0f);
          },
          weak_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(calendar_utils::kResetToTodayFadeAnimationDuration)
      .SetOpacity(header_, 1.0f)
      .SetOpacity(content_view_, 1.0f);
}

void CalendarView::UpdateHeaders() {
  header_->UpdateHeaders(
      calendar_view_controller_->GetOnScreenMonthName(),
      base::TimeFormatWithPattern(
          calendar_view_controller_->currently_shown_date(), "YYYY"));
}

void CalendarView::RestoreHeadersStatus() {
  header_->layer()->GetAnimator()->StopAnimating();
  header_->layer()->SetOpacity(1.0f);
  header_->layer()->SetTransform(gfx::Transform());
  scrolling_settled_timer_.Reset();
  if (!should_header_animate_)
    header_animation_restart_timer_.Reset();
}

void CalendarView::RestoreMonthStatus() {
  current_month_->layer()->GetAnimator()->StopAnimating();
  current_label_->layer()->GetAnimator()->StopAnimating();
  previous_month_->layer()->GetAnimator()->StopAnimating();
  previous_label_->layer()->GetAnimator()->StopAnimating();
  next_label_->layer()->GetAnimator()->StopAnimating();
  next_month_->layer()->GetAnimator()->StopAnimating();
  next_next_label_->layer()->GetAnimator()->StopAnimating();
  next_next_month_->layer()->GetAnimator()->StopAnimating();
  ResetLayer(current_month_);
  ResetLayer(current_label_);
  ResetLayer(previous_label_);
  ResetLayer(previous_month_);
  ResetLayer(next_label_);
  ResetLayer(next_month_);
  ResetLayer(next_next_label_);
  ResetLayer(next_next_month_);

  if (!should_months_animate_)
    months_animation_restart_timer_.Reset();
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
    scroll_view_->ScrollToPosition(
        scroll_view_->vertical_scroll_bar(),
        PositionOfToday() +
            (event_list_view_ ? calendar_utils::kDateVerticalPadding : 0));
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
  calendar_view_controller_->set_expanded_area_available_height(
      scroll_view_->GetVisibleRect().height() -
      calendar_view_controller_->row_height());
}

void CalendarView::OnViewFocused(View* observed_view) {
  if (observed_view == this) {
    content_view_->RequestFocus();
    SetFocusBehavior(FocusBehavior::NEVER);
    return;
  }

  if (observed_view != content_view_ || IsDateCellViewFocused())
    return;

  auto* focus_manager = GetFocusManager();
  previous_month_->EnableFocus();
  current_month_->EnableFocus();
  next_month_->EnableFocus();
  next_next_month_->EnableFocus();

  // If the event list is showing, focus on the first cell in the current row or
  // today's cell if today is in this row.
  if (event_list_view_) {
    focus_manager->SetFocusedView(
        current_month_->focused_cells()[calendar_view_controller_
                                            ->GetExpandedRowIndex()]);
    AdjustDateCellVoxBounds();

    content_view_->SetFocusBehavior(FocusBehavior::NEVER);
    return;
  }

  // When focusing on the `content_view_`, we decide which is the to-be-focused
  // cell based on the current position.
  const int position = scroll_view_->GetVisibleRect().y();
  const int row_height = calendar_view_controller_->row_height();

  // At least one row of the current month is visible on the screen. The
  // to-be-focused cell should be the first non-grayed date cell that is
  // visible, or today's cell if today is in the current month and visible.
  if (position < (next_label_->y() - row_height - kMonthVerticalPadding -
                  kLabelVerticalPadding)) {
    int row_index = 0;
    const int today_index = calendar_view_controller_->today_row() - 1;
    while (position > (PositionOfCurrentMonth() + row_index * row_height))
      ++row_index;

    CalendarDateCellView* focused_cell;
    if (current_month_->has_today() && row_index <= today_index) {
      focused_cell = current_month_->focused_cells()[today_index];
    } else {
      focused_cell = current_month_->focused_cells()[row_index];
    }
    focused_cell->SetFirstOnFocusedAccessibilityLabel();
    focus_manager->SetFocusedView(focused_cell);
  } else {
    // If there's no visible row of the current month on the screen, focus on
    // the first visible non-grayed-out date of the next month.
    focus_manager->SetFocusedView(next_month_->focused_cells().front());
  }

  AdjustDateCellVoxBounds();

  content_view_->SetFocusBehavior(FocusBehavior::NEVER);
}

views::View* CalendarView::AddLabelWithId(LabelType type, bool add_at_front) {
  auto label = std::make_unique<MonthHeaderLabelView>(
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
  if (!should_header_animate_) {
    UpdateHeaders();
    RestoreHeadersStatus();
    return;
  }

  header_->layer()->SetTransform(gfx::Transform());
  header_->layer()->SetOpacity(0.0f);
  UpdateHeaders();

  const int header_height = header_->GetPreferredSize().height();
  gfx::Vector2dF moving_location = gfx::Vector2dF(
      0, is_scrolling_up_ ? -header_height / 2 : header_height / 2);
  gfx::Transform initial_state;
  initial_state.Translate(moving_location);
  set_should_header_animate(false);

  auto header_reporter = calendar_metrics::CreateAnimationReporter(
      header_, kOnMonthChangedAnimationHistogram);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(
          [](base::WeakPtr<CalendarView> calendar_view) {
            if (!calendar_view)
              return;
            calendar_view->set_should_header_animate(true);
            calendar_view->reset_scrolling_settled_timer();
          },
          weak_factory_.GetWeakPtr()))
      .OnAborted(base::BindOnce(
          [](base::WeakPtr<CalendarView> calendar_view) {
            if (!calendar_view)
              return;
            calendar_view->UpdateHeaders();
            calendar_view->RestoreHeadersStatus();
          },
          weak_factory_.GetWeakPtr()))
      .Once()
      .SetTransform(header_, std::move(initial_state))
      .Then()
      .SetDuration(calendar_utils::kAnimationDurationForMoving)
      .SetTransform(header_, gfx::Transform(), gfx::Tween::EASE_OUT_2)
      .At(base::Milliseconds(0))
      .SetDuration(kDelayVisibilityAnimationDuration)
      .Then()
      .SetDuration(calendar_utils::kAnimationDurationForVisibility)
      .SetOpacity(header_, 1.0f);
}

void CalendarView::OnEventsFetched(
    const google_apis::calendar::EventList* events) {
  // No need to store the events, but we need to notify the month views that
  // something may have changed and they need to refresh.
  previous_month_->SchedulePaintChildren();
  current_month_->SchedulePaintChildren();
  next_month_->SchedulePaintChildren();
  next_next_month_->SchedulePaintChildren();
}

void CalendarView::OpenEventList() {
  // Don't show the the `event_list_` view for unlogged in users.
  if (!calendar_utils::IsActiveUser())
    return;

  // If the event list is already open or the months are moving/animation,
  // do nothing.
  if (event_list_view_ || is_calendar_view_scrolling_)
    return;

  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);

  // Updates `scroll_view_`'s accessible name with the selected date.
  absl::optional<base::Time> selected_date =
      calendar_view_controller_->selected_date();
  scroll_view_->GetViewAccessibility().OverrideName(l10n_util::GetStringFUTF16(
      IDS_ASH_CALENDAR_CONTENT_ACCESSIBLE_DESCRIPTION,
      base::TimeFormatWithPattern(
          calendar_view_controller_->currently_shown_date(), "MMMM yyyy"),
      base::TimeFormatWithPattern(selected_date.value(), "MMMMdyyyy")));
  scroll_view_->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged,
                                         /*send_native_event=*/true);
  event_list_view_ = AddChildView(
      std::make_unique<CalendarEventListView>(calendar_view_controller_.get()));
  event_list_view_->SetFocusBehavior(FocusBehavior::NEVER);
  event_list_view_->SetBounds(
      scroll_view_->GetVisibleRect().x(),
      scroll_view_->GetVisibleRect().bottom(),
      scroll_view_->GetVisibleRect().width(),
      calendar_view_controller_->expanded_area_available_height());

  if (!should_months_animate_) {
    OnOpenEventListAnimationComplete();
    return;
  }

  set_should_months_animate(false);
  gfx::Vector2dF moving_up_location = gfx::Vector2dF(
      0, -PositionOfSelectedDate() + scroll_view_->GetVisibleRect().y());

  gfx::Transform month_moving;
  month_moving.Translate(moving_up_location);

  gfx::Transform list_view_moving;
  list_view_moving.Translate(gfx::Vector2dF(
      0, -event_list_view_->GetBoundsInScreen().y() +
             scroll_view_->GetBoundsInScreen().bottom() -
             calendar_view_controller_->expanded_area_available_height()));

  // Tracks animation smoothness. For now, we only track animation smoothness
  // for 1 month and 1 label since all 2 month views and 2 label views are
  // similar and perform the same animation. If this is not the case in the
  // future, we should add additional metrics for the rest.
  auto month_reporter = calendar_metrics::CreateAnimationReporter(
      current_month_, kMonthViewOpenEventListAnimationHistogram);
  auto label_reporter = calendar_metrics::CreateAnimationReporter(
      current_label_, kLabelViewOpenEventListAnimationHistogram);
  auto event_list_reporter = calendar_metrics::CreateAnimationReporter(
      event_list_view_, kEventListViewOpenEventListAnimationHistogram);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(
          [](base::WeakPtr<CalendarView> calendar_view) {
            if (!calendar_view)
              return;
            calendar_view->OnOpenEventListAnimationComplete();
          },
          weak_factory_.GetWeakPtr()))
      .OnAborted(base::BindOnce(
          [](base::WeakPtr<CalendarView> calendar_view) {
            if (!calendar_view)
              return;
            calendar_view->OnOpenEventListAnimationComplete();
          },
          weak_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(calendar_utils::kAnimationDurationForMoving)
      .SetTransform(current_month_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(current_label_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(next_label_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(next_month_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(next_next_label_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(next_next_month_, month_moving, gfx::Tween::EASE_OUT_2)
      .At(base::Milliseconds(0))
      .SetDuration(kAnimationDurationForEventsMoving)
      .SetTransform(event_list_view_, std::move(list_view_moving),
                    gfx::Tween::EASE_OUT_2);
}

void CalendarView::CloseEventList() {
  // Updates `scroll_view_`'s accessible name without the selected date.
  scroll_view_->GetViewAccessibility().OverrideName(l10n_util::GetStringFUTF16(
      IDS_ASH_CALENDAR_BUBBLE_ACCESSIBLE_DESCRIPTION,
      base::TimeFormatWithPattern(
          calendar_view_controller_->currently_shown_date(), "MMMM yyyy")));
  scroll_view_->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged,
                                         /*send_native_event=*/true);
  scroll_view_->ClipHeightTo(0, INT_MAX);
  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);

  // The position of the `event_list_view_` is on the most top the calendar view
  // after the height of the `scroll_view_` is set to max. This init set it to
  // the correct position.
  const int init_position =
      event_list_view_->GetBoundsInScreen().y() - GetBoundsInScreen().y();
  gfx::Transform list_view_moving_init;
  list_view_moving_init.Translate(0, init_position);

  // Then based on the `event_list_view_`'s position, move it
  // out of the visible view.
  gfx::Transform list_view_moving;
  list_view_moving.Translate(gfx::Vector2dF(
      0, calendar_view_controller_->expanded_area_available_height() +
             init_position));

  auto event_list_reporter = calendar_metrics::CreateAnimationReporter(
      event_list_view_, kCloseEventListAnimationHistogram);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(
          [](base::WeakPtr<CalendarView> calendar_view) {
            if (!calendar_view)
              return;
            calendar_view->OnCloseEventListAnimationComplete();
          },
          weak_factory_.GetWeakPtr()))
      .OnAborted(base::BindOnce(
          [](base::WeakPtr<CalendarView> calendar_view) {
            if (!calendar_view)
              return;
            calendar_view->OnCloseEventListAnimationComplete();
          },
          weak_factory_.GetWeakPtr()))
      .Once()
      .SetTransform(event_list_view_, std::move(list_view_moving_init))
      .Then()
      .SetDuration(kAnimationDurationForClosingEvents)
      .SetTransform(event_list_view_, std::move(list_view_moving),
                    gfx::Tween::FAST_OUT_SLOW_IN);
}

void CalendarView::ScrollUpOneMonth() {
  calendar_view_controller_->UpdateMonth(
      calendar_view_controller_->GetPreviousMonthFirstDayLocal(1));
  content_view_->RemoveChildViewT(next_next_label_);
  content_view_->RemoveChildViewT(next_next_month_);

  next_next_label_ = next_label_;
  next_next_month_ = next_month_;
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
  next_label_ = next_next_label_;
  next_month_ = next_next_month_;

  next_next_label_ = AddLabelWithId(LabelType::NEXTNEXT);
  next_next_month_ = AddMonth(
      calendar_view_controller_->GetNextMonthFirstDayLocal(/*num_months=*/2));
  if (IsDateCellViewFocused())
    next_next_month_->EnableFocus();

  // Same as adding previous views. We need to remove the height of the
  // deleted month to keep the current view's position.
  int position = scroll_view_->GetVisibleRect().y() - removed_height;

  base::AutoReset<bool> is_resetting_scrolling(&is_resetting_scroll_, true);
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(), position);

  MaybeResetContentViewFocusBehavior();
}

void CalendarView::ScrollOneMonthAndAutoScroll(bool scroll_up) {
  if (is_resetting_scroll_)
    return;

  base::AutoReset<bool> is_resetting_scrolling(&is_resetting_scroll_, true);
  RestoreMonthStatus();
  if (scroll_up)
    ScrollUpOneMonth();
  else
    ScrollDownOneMonth();
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                 PositionOfCurrentMonth());
}

void CalendarView::ScrollOneMonthWithAnimation(bool scroll_up) {
  is_scrolling_up_ = scroll_up;
  if (is_resetting_scroll_)
    return;

  if (event_list_view_) {
    // If it is animating to open this `event_list_view_`, disable the up/down
    // buttons.
    if (!should_months_animate_ || !should_header_animate_)
      return;
    ScrollOneRowWithAnimation(scroll_up);
    return;
  }

  // If there's already an existing animation, restores each layer's visibility
  // and position.
  if (!should_months_animate_ || !should_header_animate_) {
    RestoreHeadersStatus();
    set_should_months_animate(false);
    set_should_header_animate(false);
    ScrollOneMonthAndAutoScroll(scroll_up);
    return;
  }

  SetShouldMonthsAnimateAndScrollEnabled(false);
  set_should_header_animate(false);
  gfx::Vector2dF moving_up_location = gfx::Vector2dF(
      0, previous_month_->GetPreferredSize().height() +
             current_label_->GetPreferredSize().height() +
             (scroll_view_->GetVisibleRect().y() - current_month_->y()));
  gfx::Vector2dF moving_down_location = gfx::Vector2dF(
      0, -current_month_->GetPreferredSize().height() -
             next_label_->GetPreferredSize().height() +
             (scroll_view_->GetVisibleRect().y() - current_month_->y()));

  gfx::Transform month_moving;
  month_moving.Translate(scroll_up ? moving_up_location : moving_down_location);

  const int header_height = header_->GetPreferredSize().height();
  const gfx::Vector2dF header_moving_location = gfx::Vector2dF(
      0, is_scrolling_up_ ? header_height / 2 : -header_height / 2);
  gfx::Transform header_moving;
  header_moving.Translate(header_moving_location);

  // Tracks animation smoothness. For now, we only track animation smoothness
  // for 1 month and 1 label since all 3 month views and 3 label views are
  // similar and perform the same animation. If this is not the case in the
  // future, we should add additional metrics for the rest.
  auto month_reporter = calendar_metrics::CreateAnimationReporter(
      current_month_, kMonthViewScrollOneMonthAnimationHistogram);
  auto label_reporter = calendar_metrics::CreateAnimationReporter(
      current_label_, kLabelViewScrollOneMonthAnimationHistogram);
  auto header_reporter = calendar_metrics::CreateAnimationReporter(
      header_, kHeaderViewScrollOneMonthAnimationHistogram);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(
          [](base::WeakPtr<CalendarView> calendar_view, bool scroll_up) {
            if (!calendar_view)
              return;
            calendar_view->set_should_header_animate(true);
            calendar_view->SetShouldMonthsAnimateAndScrollEnabled(true);
            calendar_view->ScrollOneMonthAndAutoScroll(scroll_up);
          },
          weak_factory_.GetWeakPtr(), scroll_up))
      .OnAborted(base::BindOnce(
          [](base::WeakPtr<CalendarView> calendar_view, bool scroll_up) {
            if (!calendar_view)
              return;
            calendar_view->SetShouldMonthsAnimateAndScrollEnabled(true);
            calendar_view->ScrollOneMonthAndAutoScroll(scroll_up);
          },
          weak_factory_.GetWeakPtr(), scroll_up))
      .Once()
      .SetDuration(calendar_utils::kAnimationDurationForMoving * 2)
      .SetTransform(current_month_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(current_label_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(previous_month_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(previous_label_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(next_month_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(next_label_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(next_next_month_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(next_next_label_, month_moving, gfx::Tween::EASE_OUT_2)
      .At(calendar_utils::kAnimationDurationForMoving)
      .SetDuration(calendar_utils::kAnimationDurationForMoving)
      .SetTransform(header_, std::move(header_moving), gfx::Tween::EASE_OUT_2)
      .At(calendar_utils::kAnimationDurationForMoving)
      .SetDuration(kDelayVisibilityAnimationDuration)
      .Then()
      .SetDuration(calendar_utils::kAnimationDurationForVisibility)
      .SetOpacity(header_, 0.0f);
}

void CalendarView::ScrollOneRowWithAnimation(bool scroll_up) {
  is_scrolling_up_ = scroll_up;
  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  base::AutoReset<bool> is_resetting_scrolling(&is_resetting_scroll_, true);

  // Scrolls to the last row of the previous month if it's currently on the
  // first row and scrolling up.
  if (scroll_up && calendar_view_controller_->GetExpandedRowIndex() == 0) {
    ScrollUpOneMonth();
    calendar_view_controller_->set_expanded_row_index(
        current_month_->last_row_index());
    const int row_height = calendar_view_controller_->GetExpandedRowIndex() *
                           calendar_view_controller_->row_height();
    scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                   PositionOfCurrentMonth() + row_height +
                                       calendar_utils::kDateVerticalPadding);
    scroll_view_->SetVerticalScrollBarMode(
        views::ScrollView::ScrollBarMode::kDisabled);
    return;
  }

  // Scrolls to the first row of the next month if it's currently on the
  // last row and scrolling down.
  if (!scroll_up && calendar_view_controller_->GetExpandedRowIndex() ==
                        current_month_->last_row_index()) {
    ScrollDownOneMonth();
    calendar_view_controller_->set_expanded_row_index(0);
    scroll_view_->ScrollToPosition(
        scroll_view_->vertical_scroll_bar(),
        PositionOfCurrentMonth() + calendar_utils::kDateVerticalPadding);
    scroll_view_->SetVerticalScrollBarMode(
        views::ScrollView::ScrollBarMode::kDisabled);
    return;
  }

  calendar_view_controller_->set_expanded_row_index(
      calendar_view_controller_->GetExpandedRowIndex() + (scroll_up ? -1 : 1));
  const int row_height = calendar_view_controller_->GetExpandedRowIndex() *
                         calendar_view_controller_->row_height();
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                 PositionOfCurrentMonth() + row_height +
                                     calendar_utils::kDateVerticalPadding);
  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  return;
}

void CalendarView::OnEvent(ui::Event* event) {
  if (!event->IsKeyEvent()) {
    TrayDetailedView::OnEvent(event);
    return;
  }

  auto* key_event = event->AsKeyEvent();
  auto key_code = key_event->key_code();
  auto* focus_manager = GetFocusManager();

  bool is_tab_key_pressed =
      key_event->type() == ui::EventType::ET_KEY_PRESSED &&
      views::FocusManager::IsTabTraversalKeyEvent(*key_event);

  if (is_tab_key_pressed) {
    RecordCalendarKeyboardNavigation(
        calendar_metrics::CalendarKeyboardNavigationSource::kTab);
  }

  if (!IsDateCellViewFocused()) {
    TrayDetailedView::OnEvent(event);
    return;
  }

  // When tab key is pressed, stops focusing on any `CalendarDateCellView` and
  // goes to the next focusable button in the header.
  if (is_tab_key_pressed) {
    // Set focus on `up_button_`/`event_list_view_` or null
    // pointer to escape the focusing on the date cell.
    if (key_event->IsShiftDown()) {
      up_button_->RequestFocus();
    } else if (event_list_view_) {
      // Moves focusing ring to the close button of the event list.
      event_list_view_->RequestFocus();
      focus_manager->AdvanceFocus(/*reverse=*/false);
    } else {
      scroll_view_->SetFocusBehavior(FocusBehavior::ALWAYS);
      scroll_view_->RequestFocus();
    }

    current_month_->DisableFocus();
    previous_month_->DisableFocus();
    next_month_->DisableFocus();
    next_next_month_->DisableFocus();

    TrayDetailedView::OnEvent(event);

    // Should move the focus to the next widget, so `AdvanceFocus` from the last
    // view.
    if (!key_event->IsShiftDown() && !event_list_view_) {
      focus_manager->AdvanceFocus(/*reverse=*/false);
      scroll_view_->SetFocusBehavior(FocusBehavior::NEVER);
    }
    event->StopPropagation();
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
      RecordCalendarKeyboardNavigation(
          calendar_metrics::CalendarKeyboardNavigationSource::kArrowKeys);

      auto* current_focusable_view = focus_manager->GetFocusedView();
      // Enable the scroll bar mode, in case it is disabled when the event list
      // is showing.
      scroll_view_->SetVerticalScrollBarMode(
          views::ScrollView::ScrollBarMode::kHiddenButEnabled);

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
      // After focusing on the new cell the view should have scrolled already
      // if needed, disable the scroll bar mode if the even list is showing.
      if (event_list_view_)
        scroll_view_->SetVerticalScrollBarMode(
            views::ScrollView::ScrollBarMode::kDisabled);
      const int current_height =
          scroll_view_->GetVisibleRect().y() - PositionOfCurrentMonth();
      calendar_view_controller_->set_expanded_row_index(
          current_height / calendar_view_controller_->row_height());

      AdjustDateCellVoxBounds();

      return;
    }
    case ui::VKEY_LEFT:
    case ui::VKEY_RIGHT: {
      RecordCalendarKeyboardNavigation(
          calendar_metrics::CalendarKeyboardNavigationSource::kArrowKeys);

      // Enable the scroll bar mode, in case it is disabled when the event list
      // is showing.
      scroll_view_->SetVerticalScrollBarMode(
          views::ScrollView::ScrollBarMode::kHiddenButEnabled);
      bool is_reverse = base::i18n::IsRTL() ? key_code == ui::VKEY_RIGHT
                                            : key_code == ui::VKEY_LEFT;
      focus_manager->AdvanceFocus(/*reverse=*/is_reverse);
      // After focusing on the new cell the view should have scrolled already
      // if needed, disable the scroll bar mode if the even list is showing.
      if (event_list_view_)
        scroll_view_->SetVerticalScrollBarMode(
            views::ScrollView::ScrollBarMode::kDisabled);

      const int current_height =
          scroll_view_->GetVisibleRect().y() - PositionOfCurrentMonth();
      calendar_view_controller_->set_expanded_row_index(
          current_height / calendar_view_controller_->row_height());

      AdjustDateCellVoxBounds();

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
  base::AutoReset<bool> set_is_scrolling(&is_calendar_view_scrolling_, true);

  // The scroll position is reset because it's adjusting the position when
  // adding or removing views from the `scroll_view_`. It should scroll to the
  // position we want, so we don't need to check the visible area position.
  if (is_resetting_scroll_)
    return;

  base::AutoReset<bool> disable_header_animation(&should_header_animate_,
                                                 false);
  // Scrolls to the previous month if the current label is moving down and
  // passing the top of the visible area.
  if (scroll_view_->GetVisibleRect().y() <= current_label_->y()) {
    ScrollUpOneMonth();
  } else if (scroll_view_->GetVisibleRect().y() >= next_label_->y()) {
    ScrollDownOneMonth();
  }
}

void CalendarView::OnMonthArrowButtonActivated(bool up,
                                               const ui::Event& event) {
  calendar_metrics::RecordMonthArrowButtonActivated(up, event);
  ScrollOneMonthWithAnimation(up);
  content_view_->OnMonthChanged();
}

void CalendarView::AdjustDateCellVoxBounds() {
  auto* focused_view = GetFocusManager()->GetFocusedView();
  DCHECK_EQ(focused_view->GetClassName(), CalendarDateCellView::kViewClassName);

  // When the Chrome Vox focusing box is in a `ScrollView`, the hidden content
  // height, which is `scroll_view_->GetVisibleRect().y()` should also be added.
  // Otherwise the position of the Chrome Vox box is off.
  gfx::Rect bounds = focused_view->GetBoundsInScreen();
  focused_view->GetViewAccessibility().OverrideBounds(
      gfx::RectF(bounds.x(), bounds.y() + scroll_view_->GetVisibleRect().y(),
                 bounds.width(), bounds.height()));
}

void CalendarView::OnOpenEventListAnimationComplete() {
  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  // Scrolls to the next month if the selected date is in the `next_month_`, so
  // that the `current_month_`is updated to the next month.
  if (!calendar_view_controller_->IsSelectedDateInCurrentMonth())
    ScrollDownOneMonth();
  // If still not in this month, it's in the `next_next_month_`. Doing this in a
  // while loop may cause a potential infinite loop. For example when the time
  // difference is not calculated or applied correctly, which may cause some
  // dates cannot be found in the months.
  if (!calendar_view_controller_->IsSelectedDateInCurrentMonth())
    ScrollDownOneMonth();
  base::AutoReset<bool> is_resetting_scrolling(&is_resetting_scroll_, true);
  RestoreMonthStatus();
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                 PositionOfSelectedDate());
  scroll_view_->ClipHeightTo(0, kExpandedCalendarViewHeightScale *
                                    calendar_view_controller_->row_height());
  event_list_view_->SetTransform(gfx::Transform());
  if (!should_months_animate_)
    months_animation_restart_timer_.Reset();
  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  calendar_view_controller_->OnEventListOpened();

  // Moves focusing ring to the close button of the event list if it's opened
  // from the date cell view focus.
  if (IsDateCellViewFocused()) {
    auto* focus_manager = GetFocusManager();
    event_list_view_->RequestFocus();
    focus_manager->AdvanceFocus(/*reverse=*/false);
    current_month_->DisableFocus();
    previous_month_->DisableFocus();
    next_month_->DisableFocus();
    next_next_month_->DisableFocus();
    content_view_->SetFocusBehavior(FocusBehavior::ALWAYS);
  }
}

void CalendarView::OnCloseEventListAnimationComplete() {
  RemoveChildViewT(event_list_view_);
  event_list_view_ = nullptr;
  calendar_view_controller_->OnEventListClosed();
}

BEGIN_METADATA(CalendarView, views::View)
END_METADATA

}  // namespace ash
