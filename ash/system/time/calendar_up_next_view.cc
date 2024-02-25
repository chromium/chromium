// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_up_next_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/bubble/bubble_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/typography.h"
#include "ash/system/time/calendar_event_list_item_view.h"
#include "ash/system/time/calendar_metrics.h"
#include "ash/system/time/calendar_up_next_view_background_painter.h"
#include "ash/system/time/calendar_utils.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/view_factory_internal.h"

namespace ash {
namespace {

// The insets for the calendar up next and event list item views.
constexpr int kCalendarUpNextViewStartEndMargin = 8;
constexpr gfx::Insets kContainerInsets =
    gfx::Insets::TLBR(4,
                      kCalendarUpNextViewStartEndMargin,
                      kCalendarUpNextViewStartEndMargin,
                      kCalendarUpNextViewStartEndMargin);
// Combined total margin to the left and right (start and end) of the up next
// and event list item views added together. Used in subtracting to calculate
// the label width below.
constexpr int kCombinedViewMargin =
    kCalendarUpNextViewStartEndMargin * 2 +
    calendar_utils::kEventListItemViewStartEndMargin * 2;
// At full width (displaying a single event) the label should be the tray width
// and so have no max applied.
constexpr int kLabelFullWidth = 0;
// UI spec is a fixed width for the whole up next event list item view, if
// there's more than 1 being shown. Given we're achieving this using
// `SizeToFit()` on a label, the label will need to account for the
// `kCombinedViewMargin` so we reduce those off the size.
constexpr int kLabelCappedWidth = 170 - kCombinedViewMargin;
constexpr gfx::Insets kHeaderInsets = gfx::Insets::TLBR(0, 12, 8, 0);
constexpr int kHeaderBetweenChildSpacing = 16;
constexpr int kScrollViewportCornerRadius = 18;

// Helper class for managing scrolling animations.
class ScrollingAnimation : public gfx::LinearAnimation,
                           public gfx::AnimationDelegate {
 public:
  explicit ScrollingAnimation(
      views::View* contents_view,
      gfx::AnimationContainer* bounds_animator_container,
      base::TimeDelta duration,
      const gfx::Rect start_visible_rect,
      const gfx::Rect end_visible_rect)
      : gfx::LinearAnimation(duration,
                             gfx::LinearAnimation::kDefaultFrameRate,
                             this),
        contents_view_(contents_view),
        start_visible_rect_(start_visible_rect),
        end_visible_rect_(end_visible_rect) {
    SetContainer(bounds_animator_container);
  }
  ScrollingAnimation(const ScrollingAnimation&) = delete;
  ScrollingAnimation& operator=(const ScrollingAnimation&) = delete;
  ~ScrollingAnimation() override = default;

  void AnimateToState(double state) override {
    gfx::Rect intermediary_rect(
        start_visible_rect_.x() +
            (end_visible_rect_.x() - start_visible_rect_.x()) * state,
        start_visible_rect_.y(), start_visible_rect_.width(),
        start_visible_rect_.height());

    contents_view_->ScrollRectToVisible(intermediary_rect);
  }

  void AnimationEnded(const gfx::Animation* animation) override {
    contents_view_->ScrollRectToVisible(end_visible_rect_);
  }

  void AnimationCanceled(const gfx::Animation* animation) override {
    AnimationEnded(animation);
  }

 private:
  // Owned by views hierarchy.
  const raw_ptr<views::View> contents_view_;
  const gfx::Rect start_visible_rect_;
  const gfx::Rect end_visible_rect_;
};

std::unique_ptr<views::Button> CreateTodaysEventsButton(
    views::Button::PressedCallback callback) {
  return views::Builder<views::Button>(
             std::make_unique<IconButton>(
                 std::move(callback), IconButton::Type::kXSmallFloating,
                 &kCalendarUpNextTodaysEventsButtonIcon,
                 IDS_ASH_CALENDAR_UP_NEXT_TODAYS_EVENTS_BUTTON))
      .Build();
}

std::unique_ptr<views::Label> CreateHeaderLabel() {
  return views::Builder<views::Label>(
             bubble_utils::CreateLabel(
                 TypographyToken::kCrosHeadline1,
                 l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_UP_NEXT),
                 cros_tokens::kCrosSysOnSurface))
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .Build();
}

bool IsRightScrollButtonEnabled(views::ScrollView* scroll_view) {
  const int contents_width =
      scroll_view->contents()->GetContentsBounds().width();
  const int scroll_position = scroll_view->GetVisibleRect().x();
  const int scroll_view_width = scroll_view->width();

  return (contents_width > scroll_view_width) &&
         (scroll_position < (contents_width - scroll_view_width));
}

// Returns the index of the first (left-most) visible (partially or wholly)
// child in the ScrollView.
int GetFirstVisibleChildIndex(
    std::vector<raw_ptr<views::View, VectorExperimental>> event_views,
    views::View* scroll_view) {
  for (size_t i = 0; i < event_views.size(); ++i) {
    auto* child = event_views[i].get();
    if (scroll_view->GetBoundsInScreen().Intersects(
            child->GetBoundsInScreen())) {
      return i;
    }
  }

  return 0;
}

// Checks if both lists contain the same events by comparing first their event
// IDs and then that they start and end at the same time. The event IDs should
// be the same (unique per Calendar) and in the same order.
bool SameEvents(const std::list<google_apis::calendar::CalendarEvent>& a,
                const std::list<google_apis::calendar::CalendarEvent>& b) {
  return base::ranges::equal(a, b, [](const auto& a, const auto& b) {
    return a.id() == b.id() &&
           a.start_time().date_time() == b.start_time().date_time() &&
           a.end_time().date_time() == b.end_time().date_time();
  });
}

}  // namespace

CalendarUpNextView::CalendarUpNextView(
    CalendarViewController* calendar_view_controller,
    views::Button::PressedCallback callback)
    : calendar_view_controller_(calendar_view_controller),
      todays_events_button_container_(
          AddChildView(std::make_unique<views::View>())),
      header_view_(AddChildView(std::make_unique<views::View>())),
      scroll_view_(AddChildView(std::make_unique<views::ScrollView>(
          views::ScrollView::ScrollWithLayers::kEnabled))),
      content_view_(scroll_view_->SetContents(std::make_unique<views::View>())),
      bounds_animator_(this) {
  SetBackground(std::make_unique<CalendarUpNextViewBackground>(
      cros_tokens::kCrosSysSystemOnBaseOpaque));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kContainerInsets, 0));
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  if (!gfx::Animation::ShouldRenderRichAnimation()) {
    bounds_animator_.SetAnimationDuration(base::TimeDelta());
  }

  on_contents_scrolled_subscription_ =
      scroll_view_->AddContentsScrolledCallback(
          base::BindRepeating(&CalendarUpNextView::ToggleScrollButtonState,
                              base::Unretained(this)));

  // Todays events button
  todays_events_button_container_
      ->SetLayoutManager(std::make_unique<views::BoxLayout>())
      ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  todays_events_button_container_->AddChildView(
      CreateTodaysEventsButton(std::move(callback)));

  // Header.
  auto* header_layout_manager =
      header_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, kHeaderInsets,
          kHeaderBetweenChildSpacing));
  header_layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  // Header label.
  auto* header_label = header_view_->AddChildView(CreateHeaderLabel());
  header_layout_manager->SetFlexForView(header_label, 1);
  // Header scroll buttons.
  auto button_container =
      views::Builder<views::View>()
          .SetLayoutManager(std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
              kHeaderBetweenChildSpacing))
          .Build();
  left_scroll_button_ =
      button_container->AddChildView(std::make_unique<IconButton>(
          base::BindRepeating(&CalendarUpNextView::OnScrollLeftButtonPressed,
                              base::Unretained(this)),
          IconButton::Type::kMediumFloating, &kCaretLeftIcon,
          IDS_ASH_CALENDAR_UP_NEXT_SCROLL_LEFT_BUTTON));
  left_scroll_button_->SetFocusBehavior(FocusBehavior::NEVER);
  right_scroll_button_ =
      button_container->AddChildView(std::make_unique<IconButton>(
          base::BindRepeating(&CalendarUpNextView::OnScrollRightButtonPressed,
                              base::Unretained(this)),
          IconButton::Type::kMediumFloating, &kCaretRightIcon,
          IDS_ASH_CALENDAR_UP_NEXT_SCROLL_RIGHT_BUTTON));
  right_scroll_button_->SetFocusBehavior(FocusBehavior::NEVER);
  header_view_->AddChildView(std::move(button_container));

  // Scroll view.
  scroll_view_->SetAllowKeyboardScrolling(false);
  scroll_view_->SetBackgroundColor(std::nullopt);
  scroll_view_->SetDrawOverflowIndicator(false);
  scroll_view_->SetViewportRoundedCornerRadius(
      gfx::RoundedCornersF(kScrollViewportCornerRadius));
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  scroll_view_->SetTreatAllScrollEventsAsHorizontal(true);
  // Set the `scroll_view_` contents to receive focus first, followed by the
  // todays events button.
  scroll_view_->InsertBeforeInFocusList(todays_events_button_container_);

  // Contents.
  content_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      calendar_utils::kUpNextBetweenChildSpacing));

  // Populate the contents of the scroll view.
  RefreshEvents();
}

CalendarUpNextView::~CalendarUpNextView() = default;

SkPath CalendarUpNextView::GetClipPath() const {
  return CalendarUpNextViewBackground::GetPath(size());
}

void CalendarUpNextView::RefreshEvents() {
  UpdateEvents(calendar_view_controller_->UpcomingEvents());
}

void CalendarUpNextView::Layout(PassKey) {
  // For some reason the `content_view_` is constrained to the
  // `scroll_view_` width and so it isn't scrollable. This seems to be a
  // problem with horizontal `ScrollView`s as this doesn't happen if you
  // make this view vertically scrollable. To make the content
  // scrollable, we need to set it's preferred size here so it's bigger
  // than the `scroll_view_` and therefore scrolls. See
  // https://crbug.com/1384131.
  // For single events we want it to be constrained to the scroll view width.
  if (content_view_ && displayed_events_.size() > 1) {
    content_view_->SizeToPreferredSize();
  }

  // `content_view_` is a child of this class so we need to Layout after
  // changing its width.
  LayoutSuperclass<views::View>(this);

  // After laying out the `content_view_`, we need to set the initial scroll
  // button state.
  ToggleScrollButtonState();
}

void CalendarUpNextView::UpdateEvents(
    const std::list<google_apis::calendar::CalendarEvent>& events) {
  if (SameEvents(displayed_events_, events)) {
    return;
  }

  displayed_events_ = events;
  content_view_->RemoveAllChildViews();

  calendar_metrics::RecordUpNextEventCount(events.size());

  const auto now = base::Time::NowFromSystemTime();
  const auto [selected_date_midnight, selected_date_midnight_utc] =
      calendar_utils::GetMidnight(now);

  // Single events are displayed filling the whole width of the tray.
  if (events.size() == 1) {
    const auto event = events.back();
    auto* child_view =
        content_view_->AddChildView(std::make_unique<CalendarEventListItemView>(
            calendar_view_controller_,
            SelectedDateParams{now, selected_date_midnight,
                               selected_date_midnight_utc},
            /*event=*/event, /*ui_params=*/
            UIParams{/*round_top_corners=*/true,
                     /*round_bottom_corners=*/true,
                     /*is_up_next_event_list_item=*/true,
                     /*show_event_list_dot=*/false,
                     /*fixed_width=*/kLabelFullWidth},
            /*event_list_item_index=*/
            EventListItemIndex{/*item_index=*/1,
                               /*total_count_of_events=*/1}));

    static_cast<views::BoxLayout*>(content_view_->GetLayoutManager())
        ->SetFlexForView(child_view, 1);

    // Hide scroll buttons if we have a single event.
    left_scroll_button_->SetVisible(false);
    right_scroll_button_->SetVisible(false);

    content_view_->InvalidateLayout();

    return;
  }

  // Multiple events are displayed in a scroll view of events with a max item
  // width. Longer event names will have an ellipsis applied.
  const int events_size = events.size();
  for (auto it = events.begin(); it != events.end(); ++it) {
    const int event_index = std::distance(events.begin(), it) + 1;
    content_view_->AddChildView(std::make_unique<CalendarEventListItemView>(
        calendar_view_controller_,
        SelectedDateParams{now, selected_date_midnight,
                           selected_date_midnight_utc},
        /*event=*/*it,
        /*ui_params=*/
        UIParams{/*round_top_corners=*/true, /*round_bottom_corners=*/true,
                 /*is_up_next_event_list_item=*/true,
                 /*show_event_list_dot=*/false,
                 /*fixed_width=*/kLabelCappedWidth},
        /*event_list_item_index=*/
        EventListItemIndex{/*item_index=*/event_index,
                           /*total_count_of_events=*/events_size}));
  }

  // Show scroll buttons if we have multiple events.
  left_scroll_button_->SetVisible(true);
  right_scroll_button_->SetVisible(true);

  content_view_->InvalidateLayout();
}

void CalendarUpNextView::OnScrollLeftButtonPressed(const ui::Event& event) {
  const Views& event_views = content_view_->children();
  if (event_views.empty()) {
    return;
  }

  calendar_metrics::RecordScrollEventInUpNext();

  const int first_visible_child_index =
      GetFirstVisibleChildIndex(event_views, scroll_view_);
  views::View* first_visible_child = event_views[first_visible_child_index];

  // If first visible child is partially visible, then just scroll to make it
  // visible.
  if (first_visible_child->GetVisibleBounds().width() !=
      first_visible_child->GetContentsBounds().width()) {
    const auto offset =
        base::i18n::IsRTL()
            ? scroll_view_->GetBoundsInScreen().right() -
                  first_visible_child->GetBoundsInScreen().right()
            : first_visible_child->GetBoundsInScreen().x() -
                  scroll_view_->GetBoundsInScreen().x();
    ScrollViewByOffset(offset);

    return;
  }

  // Otherwise, find the child before that and scroll to it.
  const int previous_child_index = first_visible_child_index - 1;
  const int index = std::max(0, previous_child_index);
  views::View* previous_child = event_views[index];
  const auto offset = base::i18n::IsRTL()
                          ? scroll_view_->GetBoundsInScreen().right() -
                                previous_child->GetBoundsInScreen().right()
                          : previous_child->GetBoundsInScreen().x() -
                                scroll_view_->GetBoundsInScreen().x();
  ScrollViewByOffset(offset);
}

void CalendarUpNextView::OnScrollRightButtonPressed(const ui::Event& event) {
  const Views& event_views = content_view_->children();
  if (event_views.empty()) {
    return;
  }

  calendar_metrics::RecordScrollEventInUpNext();

  const int first_visible_child_index =
      GetFirstVisibleChildIndex(event_views, scroll_view_);

  // When scrolling right, the next event should be aligned to the left of the
  // scroll view. The amount to offset is calculated by getting the visible
  // bounds of the first visible child + the between child spacing. Using the
  // visible bounds means this handles partially or fully visible views and we
  // scroll past them i.e. the amount of space the first visible event takes up
  // so the next one lines up nicely.
  const int first_child_offset =
      (event_views[first_visible_child_index]->GetVisibleBounds().width() +
       calendar_utils::kUpNextBetweenChildSpacing);
  // Calculate the max scroll position based on how far along we've scrolled.
  // `ScrollByOffset` will go way past the size of the contents so we need to
  // constrain it to go no further than the end of the content view.
  const int max_scroll_position = content_view_->GetContentsBounds().width() -
                                  scroll_view_->GetVisibleRect().right();
  const int offset = std::min(max_scroll_position, first_child_offset);

  ScrollViewByOffset(offset);
}

void CalendarUpNextView::ToggleScrollButtonState() {
  // Enable the scroll view buttons if there is a position to scroll to.
  left_scroll_button_->SetEnabled(scroll_view_->GetVisibleRect().x() > 0);
  right_scroll_button_->SetEnabled(IsRightScrollButtonEnabled(scroll_view_));
}

void CalendarUpNextView::ScrollViewByOffset(int offset) {
  std::optional<gfx::Rect> visible_content_rect =
      scroll_view_->GetVisibleRect();
  if (!visible_content_rect.has_value() || offset == 0) {
    return;
  }

  // Set the `start_edge` depending on the offset.
  // If the offset is negative ie. we're scrolling left, we should use the x
  // coordinate of the scroll viewport as the `start_edge` to base our offset
  // on. If the offset is positive i.e. we're scrolling right, then we should
  // use the right coordinate of the viewport.
  int start_edge =
      (offset > 0) ? visible_content_rect->right() : visible_content_rect->x();

  AnimateScrollToShowXCoordinate(start_edge, start_edge + offset);
}

void CalendarUpNextView::AnimateScrollToShowXCoordinate(const int start_edge,
                                                        const int target_edge) {
  if (scrolling_animation_) {
    scrolling_animation_->Stop();
  }

  scrolling_animation_ = std::make_unique<ScrollingAnimation>(
      content_view_, bounds_animator_.container(),
      bounds_animator_.GetAnimationDuration(),
      /*start_visible_rect=*/gfx::Rect(start_edge, 0, 0, 0),
      /*end_visible_rect=*/gfx::Rect(target_edge, 0, 0, 0));
  scrolling_animation_->Start();
}

BEGIN_METADATA(CalendarUpNextView);
END_METADATA

}  // namespace ash
