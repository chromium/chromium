// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_view.h"

#include <algorithm>
#include <climits>
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/message_center/ash_message_center_lock_screen_controller.h"
#include "ash/system/message_center/message_center_constants.h"
#include "ash/system/message_center/message_center_scroll_bar.h"
#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/notification_center/notification_list_view.h"
#include "ash/system/notification_center/stacked_notification_bar.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/user_metrics.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Inset the top and the bottom of the scroll bar so it won't be clipped by
// rounded corners.
constexpr auto kScrollBarInsets = gfx::Insets::TLBR(16, 0, 16, 0);

constexpr base::TimeDelta kHideStackingBarAnimationDuration =
    base::Milliseconds(330);
constexpr base::TimeDelta kCollapseAnimationDuration = base::Milliseconds(640);

class ScrollerContentsView : public views::View {
 public:
  explicit ScrollerContentsView(NotificationListView* notification_list_view) {
    auto* contents_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    contents_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStretch);
    AddChildView(notification_list_view);
  }

  ScrollerContentsView(const ScrollerContentsView&) = delete;
  ScrollerContentsView& operator=(const ScrollerContentsView&) = delete;

  ~ScrollerContentsView() override = default;

  // views::View:
  void ChildPreferredSizeChanged(views::View* view) override {
    PreferredSizeChanged();
  }

  const char* GetClassName() const override { return "ScrollerContentsView"; }
};

}  // namespace

NotificationCenterView::NotificationCenterView(
    UnifiedSystemTrayView* parent,
    scoped_refptr<UnifiedSystemTrayModel> model,
    UnifiedMessageCenterBubble* bubble)
    : parent_(parent),
      model_(model),
      message_center_bubble_(bubble),
      notification_bar_(new StackedNotificationBar(this)),
      // TODO(crbug.com/1247455): Determine how to use ScrollWithLayers without
      // breaking ARC.
      scroller_(new views::ScrollView()),
      notification_list_view_(new NotificationListView(this, model)),
      last_scroll_position_from_bottom_(0),
      animation_(std::make_unique<gfx::LinearAnimation>(this)),
      focus_search_(std::make_unique<views::FocusSearch>(this, false, false)) {
  auto* scroll_bar = new MessageCenterScrollBar(this);
  scroll_bar->SetInsets(kScrollBarInsets);
  scroll_bar_ = scroll_bar;

  layout_manager_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(kMessageCenterPadding)));
}

NotificationCenterView::~NotificationCenterView() {
  if (!features::IsQsRevampEnabled()) {
    // `NotificationCenterView` should always open with the newest notification
    // on top with QsRevamp enabled so we do not need to store the scroll state.
    model_->set_notification_target_mode(
        UnifiedSystemTrayModel::NotificationTargetMode::LAST_NOTIFICATION);

    RemovedFromWidget();
  }

  scroller_->RemoveObserver(this);
}

void NotificationCenterView::Init() {
  notification_list_view_->Init();

  // Need to set the transparent background explicitly, since ScrollView has
  // set the default opaque background color.
  // TODO(crbug.com/1247455): Be able to do
  // SetContentsLayerType(LAYER_NOT_DRAWN).
  scroller_->SetContents(
      std::make_unique<ScrollerContentsView>(notification_list_view_));
  scroller_->SetBackgroundColor(absl::nullopt);
  scroller_->SetVerticalScrollBar(base::WrapUnique(scroll_bar_.get()));
  scroller_->SetDrawOverflowIndicator(false);
  scroller_->SetPaintToLayer();
  scroller_->layer()->SetRoundedCornerRadius(gfx::RoundedCornersF{
      static_cast<float>(chromeos::features::IsJellyEnabled()
                             ? kJellyMessageCenterScrollViewCornerRadius
                             : kMessageCenterScrollViewCornerRadius)});

  AddChildView(scroller_.get());

  // Make sure the scroll view takes up the entirety of available height in the
  // revamped notification center view. With the QsRevamp we do not manually
  // calculate sizes for any of the views, only relying on a max height
  // constraint for the `TrayBubbleView` so we need to set flex for the scroll
  // view here.
  if (features::IsQsRevampEnabled()) {
    scroller_->AddObserver(this);
    scroller_->ClipHeightTo(0, INT_MAX);
    layout_manager_->SetFlexForView(scroller_, 1);

    on_contents_scrolled_subscription_ = scroller_->AddContentsScrolledCallback(
        base::BindRepeating(&NotificationCenterView::OnContentsScrolled,
                            base::Unretained(this)));
  }

  AddChildView(notification_bar_.get());
}

bool NotificationCenterView::UpdateNotificationBar() {
  return notification_bar_->Update(
      notification_list_view_->GetTotalNotificationCount(),
      notification_list_view_->GetTotalPinnedNotificationCount(),
      GetStackedNotifications());
}

void NotificationCenterView::SetMaxHeight(int max_height) {
  // Not applicable when the QsRevamp feature is enabled since the notification
  // center can take up the entire work area's height.
  if (features::IsQsRevampEnabled()) {
    return;
  }

  int max_scroller_height = max_height -
                            notification_bar_->GetPreferredSize().height() -
                            2 * kMessageCenterPadding;
  scroller_->ClipHeightTo(0, max_scroller_height);
}

void NotificationCenterView::SetAvailableHeight(int available_height) {
  available_height_ = available_height;
  UpdateVisibility();
}

void NotificationCenterView::SetExpanded() {
  if (!collapsed_) {
    return;
  }

  collapsed_ = false;
  notification_bar_->SetExpanded();
  scroller_->SetVisible(true);
}

void NotificationCenterView::SetCollapsed(bool animate) {
  if (!GetVisible() || collapsed_) {
    return;
  }

  collapsed_ = true;
  if (animate) {
    StartCollapseAnimation();
  } else {
    scroller_->SetVisible(false);
    notification_bar_->SetCollapsed();
  }
}

void NotificationCenterView::ClearAllNotifications() {
  base::RecordAction(
      base::UserMetricsAction("StatusArea_Notifications_StackingBarClearAll"));

  notification_list_view_->ClearAllWithAnimation();
}

void NotificationCenterView::ExpandMessageCenter() {
  // With QsRevamp enabled the `NotificationCenterView` only has a single fully
  // expanded state so we do not need this toggle.
  DCHECK(!features::IsQsRevampEnabled());

  base::RecordAction(
      base::UserMetricsAction("StatusArea_Notifications_SeeAllNotifications"));
  message_center_bubble_->ExpandMessageCenter();
}

bool NotificationCenterView::IsScrollBarVisible() const {
  return scroll_bar_->GetVisible();
}

void NotificationCenterView::OnNotificationSlidOut() {
  UpdateNotificationBar();

  if (!notification_list_view_->GetTotalNotificationCount()) {
    StartCollapseAnimation();
  }
}

void NotificationCenterView::ListPreferredSizeChanged() {
  UpdateVisibility();
  PreferredSizeChanged();
  SetMaxHeight(available_height_);

  if (GetWidget() && !GetWidget()->IsClosed()) {
    GetWidget()->SynthesizeMouseMoveEvent();
  }
}

void NotificationCenterView::ConfigureMessageView(
    message_center::MessageView* message_view) {
  message_view->set_scroller(scroller_);
}

void NotificationCenterView::AddedToWidget() {
  // No custom focus behavior needed with QsRevamp enabled so we do not need to
  // add a focus change listener.
  if (features::IsQsRevampEnabled()) {
    return views::View::AddedToWidget();
  }

  focus_manager_ = GetFocusManager();
  if (focus_manager_) {
    focus_manager_->AddFocusChangeListener(this);
  }
}

void NotificationCenterView::RemovedFromWidget() {
  if (features::IsQsRevampEnabled()) {
    return views::View::RemovedFromWidget();
  }

  if (!focus_manager_) {
    return;
  }
  focus_manager_->RemoveFocusChangeListener(this);
  focus_manager_ = nullptr;
}

void NotificationCenterView::OnViewBoundsChanged(views::View* observed_view) {
  UpdateNotificationBar();
}

void NotificationCenterView::OnMessageCenterScrolled() {
  if (features::IsQsRevampEnabled()) {
    return;
  }

  last_scroll_position_from_bottom_ =
      scroll_bar_->GetMaxPosition() - scroller_->GetVisibleRect().y();

  DCHECK(model_);

  // Reset the target if user scrolls the list manually.
  model_->set_notification_target_mode(
      UnifiedSystemTrayModel::NotificationTargetMode::LAST_POSITION);

  bool was_count_updated = UpdateNotificationBar();
  if (was_count_updated) {
    const int previous_y = scroller_->y();
    // Adjust scroll position when counter visibility is changed so that
    // on-screen position of notification list does not change.
    scroll_bar_->ScrollByContentsOffset(previous_y - scroller_->y());
  }
}

void NotificationCenterView::OnWillChangeFocus(views::View* before,
                                               views::View* now) {}

void NotificationCenterView::OnDidChangeFocus(views::View* before,
                                              views::View* now) {
  // There should be no special case behavior for focus changes once the
  // QsRevamp feature is enabled.
  if (features::IsQsRevampEnabled()) {
    return;
  }

  if (notification_list_view_->is_deleting_removed_notifications()) {
    return;
  }

  OnMessageCenterScrolled();

  if (!collapsed()) {
    views::View* first_view = GetFirstFocusableChild();
    views::View* last_view = GetLastFocusableChild();

    // If we are cycling back to the first view from the last view or vice
    // verse. Focus out of the message center to the quick settings bubble. The
    // direction of the cycle determines where the focus will move to in quick
    // settings.
    bool focused_out = false;
    if (before == last_view && now == first_view) {
      focused_out = message_center_bubble_->FocusOut(false /* reverse */);
    } else if (before == first_view && now == last_view) {
      focused_out = message_center_bubble_->FocusOut(true /* reverse */);
    }

    // Clear the focus state completely for the message center.
    // We acquire the focus back from the quick settings widget based on the
    // cycling direction.
    if (focused_out) {
      GetFocusManager()->ClearFocus();
      GetFocusManager()->SetStoredFocusView(nullptr);
    }
  }
}

void NotificationCenterView::AnimationEnded(const gfx::Animation* animation) {
  // This is also called from AnimationCanceled().
  animation_->SetCurrentValue(1.0);
  PreferredSizeChanged();

  animation_state_ = NotificationCenterAnimationState::IDLE;
  notification_bar_->SetAnimationState(animation_state_);
  UpdateVisibility();
}

void NotificationCenterView::AnimationProgressed(
    const gfx::Animation* animation) {
  // Make the scroller containing notifications invisible and change the
  // notification bar to it's collapsed state in the middle of the animation to
  // the collapsed state.
  if (collapsed_ && scroller_->GetVisible() &&
      animation_->GetCurrentValue() >= 0.5) {
    scroller_->SetVisible(false);
    notification_bar_->SetCollapsed();
  }
  PreferredSizeChanged();
}

void NotificationCenterView::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void NotificationCenterView::OnContentsScrolled() {
  UpdateNotificationBar();
}

void NotificationCenterView::StartHideStackingBarAnimation() {
  animation_->End();
  animation_state_ = NotificationCenterAnimationState::HIDE_STACKING_BAR;
  notification_bar_->SetAnimationState(animation_state_);
  animation_->SetDuration(kHideStackingBarAnimationDuration);
  animation_->Start();
}

void NotificationCenterView::StartCollapseAnimation() {
  animation_->End();
  animation_state_ = NotificationCenterAnimationState::COLLAPSE;
  notification_bar_->SetAnimationState(animation_state_);
  animation_->SetDuration(kCollapseAnimationDuration);
  animation_->Start();
}

double NotificationCenterView::GetAnimationValue() const {
  return gfx::Tween::CalculateValue(gfx::Tween::FAST_OUT_SLOW_IN,
                                    animation_->GetCurrentValue());
}

void NotificationCenterView::UpdateVisibility() {
  // With QsRevamp enabled the visibility of the bubble will be tied to the
  // `NotificationCenterTray` so we do not need to make any visibility changes
  // here.
  if (features::IsQsRevampEnabled()) {
    return;
  }

  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();

  SetVisible(available_height_ >= kUnifiedNotificationMinimumHeight &&
             (animation_state_ == NotificationCenterAnimationState::COLLAPSE ||
              notification_list_view_->GetPreferredSize().height() > 0) &&
             session_controller->ShouldShowNotificationTray() &&
             (!session_controller->IsScreenLocked() ||
              AshMessageCenterLockScreenController::IsEnabled()));

  DCHECK(model_);
  if (!GetVisible()) {
    // When notification list went invisible, the last notification should be
    // targeted next time.
    model_->set_notification_target_mode(
        UnifiedSystemTrayModel::NotificationTargetMode::LAST_NOTIFICATION);

    // Transfer focus to quick settings when going invisible.
    auto* widget = GetWidget();
    if (widget && widget->IsActive()) {
      widget->GetFocusManager()->ClearFocus();
      message_center_bubble_->ActivateQuickSettingsBubble();
    }
  }
}

void NotificationCenterView::ScrollToTarget() {
  // With QsRevamp enabled we do not need to store the scroll position so this
  // entire function should become redundant.
  DCHECK(!features::IsQsRevampEnabled());

  // Following logic doesn't work when the view is invisible, because it uses
  // the height of |scroller_|.
  if (!GetVisible()) {
    return;
  }

  DCHECK(model_);

  auto target_mode = model_->notification_target_mode();

  // Notification views may be deleted during an animation, so wait until it
  // finishes before scrolling to a new target (see crbug.com/954001).
  if (notification_list_view_->IsAnimating()) {
    target_mode = UnifiedSystemTrayModel::NotificationTargetMode::LAST_POSITION;
  }

  int position;
  switch (target_mode) {
    case UnifiedSystemTrayModel::NotificationTargetMode::LAST_POSITION:
      // Restore the previous scrolled position with matching the distance from
      // the bottom.
      position =
          scroll_bar_->GetMaxPosition() - last_scroll_position_from_bottom_;
      break;
    case UnifiedSystemTrayModel::NotificationTargetMode::NOTIFICATION_ID:
      [[fallthrough]];
    case UnifiedSystemTrayModel::NotificationTargetMode::LAST_NOTIFICATION: {
      const gfx::Rect& target_rect =
          (model_->notification_target_mode() ==
           UnifiedSystemTrayModel::NotificationTargetMode::NOTIFICATION_ID)
              ? notification_list_view_->GetNotificationBounds(
                    model_->notification_target_id())
              : notification_list_view_->GetLastNotificationBounds();

      const int last_notification_offset =
          target_rect.height() - scroller_->height();
      if (last_notification_offset > 0) {
        // If the target notification is taller than |scroller_|, we should
        // align the top of the notification with the top of |scroller_|.
        position = target_rect.y();
      } else {
        // Otherwise, we align the bottom of the notification with the bottom of
        // |scroller_|;
        position = target_rect.bottom() - scroller_->height();
      }
    }
  }

  scroller_->ScrollToPosition(scroll_bar_, position);
  UpdateNotificationBar();
  last_scroll_position_from_bottom_ =
      scroll_bar_->GetMaxPosition() - scroller_->GetVisibleRect().y();
}

std::vector<message_center::Notification*>
NotificationCenterView::GetStackedNotifications() const {
  // CountNotificationsAboveY() only works after SetBoundsRect() is called at
  // least once.
  if (scroller_->bounds().IsEmpty()) {
    scroller_->SetBoundsRect(GetContentsBounds());
  }

  if (collapsed_) {
    // When in collapsed state, all notifications are hidden, so all
    // notifications are stacked.
    return notification_list_view_->GetAllNotifications();
  }

  const int y_offset = scroller_->GetVisibleRect().bottom() - scroller_->y();
  return notification_list_view_->GetNotificationsBelowY(y_offset);
}

std::vector<std::string>
NotificationCenterView::GetNonVisibleNotificationIdsInViewHierarchy() const {
  // CountNotificationsAboveY() only works after SetBoundsRect() is called at
  // least once.
  if (scroller_->bounds().IsEmpty()) {
    scroller_->SetBoundsRect(GetContentsBounds());
  }
  if (collapsed_) {
    // When in collapsed state, all notifications are hidden, so all
    // notifications are stacked.
    return notification_list_view_->GetAllNotificationIds();
  }

  const int y_offset_above = scroller_->GetVisibleRect().y() - scroller_->y() +
                             kStackedNotificationBarHeight;
  auto above_id_list =
      notification_list_view_->GetNotificationIdsAboveY(y_offset_above);
  const int y_offset_below =
      scroller_->GetVisibleRect().bottom() - scroller_->y();
  const auto below_id_list =
      notification_list_view_->GetNotificationIdsBelowY(y_offset_below);
  above_id_list.insert(above_id_list.end(), below_id_list.begin(),
                       below_id_list.end());
  return above_id_list;
}

void NotificationCenterView::FocusOut(bool reverse) {
  // No customized focus behavior with QsRevamp.
  DCHECK(!features::IsQsRevampEnabled());

  if (message_center_bubble_ && message_center_bubble_->FocusOut(reverse)) {
    GetFocusManager()->ClearFocus();
    GetFocusManager()->SetStoredFocusView(nullptr);
  }
}

void NotificationCenterView::FocusEntered(bool reverse) {
  // No customized focus behavior with QsRevamp.
  DCHECK(!features::IsQsRevampEnabled());

  views::View* focus_view =
      reverse ? GetLastFocusableChild() : GetFirstFocusableChild();
  GetFocusManager()->ClearFocus();
  GetFocusManager()->SetFocusedView(focus_view);
}

views::View* NotificationCenterView::GetFirstFocusableChild() {
  views::FocusTraversable* dummy_focus_traversable;
  views::View* dummy_focus_traversable_view;
  return focus_search_->FindNextFocusableView(
      nullptr, views::FocusSearch::SearchDirection::kForwards,
      views::FocusSearch::TraversalDirection::kDown,
      views::FocusSearch::StartingViewPolicy::kSkipStartingView,
      views::FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
      &dummy_focus_traversable, &dummy_focus_traversable_view);
}

views::View* NotificationCenterView::GetLastFocusableChild() {
  views::FocusTraversable* focus_traversable = nullptr;
  views::View* dummy_focus_traversable_view = nullptr;
  views::View* last_view = focus_search_->FindNextFocusableView(
      nullptr, views::FocusSearch::SearchDirection::kBackwards,
      views::FocusSearch::TraversalDirection::kDown,
      views::FocusSearch::StartingViewPolicy::kSkipStartingView,
      views::FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
      &focus_traversable, &dummy_focus_traversable_view);

  if (last_view || !focus_traversable) {
    return last_view;
  }

  return focus_traversable->GetFocusSearch()->FindNextFocusableView(
      nullptr, views::FocusSearch::SearchDirection::kBackwards,
      views::FocusSearch::TraversalDirection::kDown,
      views::FocusSearch::StartingViewPolicy::kSkipStartingView,
      views::FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
      &focus_traversable, &dummy_focus_traversable_view);
}

BEGIN_METADATA(NotificationCenterView, views::View);
END_METADATA

}  // namespace ash
