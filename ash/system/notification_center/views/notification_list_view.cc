// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/views/notification_list_view.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/message_center/arc_notification_constants.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ash/system/notification_center/message_center_utils.h"
#include "ash/system/notification_center/message_view_factory.h"
#include "ash/system/notification_center/metrics_utils.h"
#include "ash/system/notification_center/notification_style_utils.h"
#include "ash/system/notification_center/views/ash_notification_view.h"
#include "ash/system/notification_center/views/message_view_container.h"
#include "ash/system/notification_center/views/notification_center_view.h"
#include "ash/system/notification_center/views/notification_swipe_control_view.h"
#include "ash/system/tray/tray_constants.h"
#include "base/auto_reset.h"
#include "base/containers/adapters.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notification_view_controller.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

using message_center::MessageCenter;
using message_center::MessageView;
using message_center::Notification;

namespace ash {

namespace {

constexpr base::TimeDelta kClosingAnimationDuration = base::Milliseconds(320);
constexpr base::TimeDelta kClearAllStackedAnimationDuration =
    base::Milliseconds(40);
constexpr base::TimeDelta kClearAllVisibleAnimationDuration =
    base::Milliseconds(160);

constexpr char kMoveDownAnimationSmoothnessHistogramName[] =
    "Ash.Notification.MoveDown.AnimationSmoothness";
constexpr char kClearAllStackedAnimationSmoothnessHistogramName[] =
    "Ash.Notification.ClearAllStacked.AnimationSmoothness";
constexpr char kClearAllVisibleAnimationSmoothnessHistogramName[] =
    "Ash.Notification.ClearAllVisible.AnimationSmoothness";
constexpr char kExpandOrCollapseAnimationSmoothnessHistogramName[] =
    "Ash.Notification.ExpandOrCollapse.AnimationSmoothness";

void RecordAnimationSmoothness(const std::string& histogram_name,
                               int smoothness) {
  base::UmaHistogramPercentage(histogram_name, smoothness);
}

void SetupThroughputTrackerForAnimationSmoothness(
    views::Widget* widget,
    std::optional<ui::ThroughputTracker>& tracker,
    const char* histogram_name) {
  // `widget` may not exist in tests.
  if (!widget) {
    return;
  }

  tracker.emplace(widget->GetCompositor()->RequestNewThroughputTracker());
  tracker->Start(ash::metrics_util::ForSmoothnessV3(
      base::BindRepeating(&RecordAnimationSmoothness, histogram_name)));
}

}  // namespace

NotificationListView::NotificationListView(
    NotificationCenterView* message_center_view)
    : views::AnimationDelegateViews(this),
      message_center_view_(message_center_view),
      animation_(std::make_unique<gfx::LinearAnimation>(this)),
      message_view_width_(GetNotificationInMessageCenterWidth()) {
  SetID(VIEW_ID_NOTIFICATION_BUBBLE_NOTIFICATION_LIST);
  if (!features::IsNotificationCenterControllerEnabled()) {
    message_center_observation_.Observe(MessageCenter::Get());
  }
  animation_->SetCurrentValue(1.0);
}

NotificationListView::~NotificationListView() = default;

void NotificationListView::Init() {
  CHECK(!features::IsNotificationCenterControllerEnabled());
  Init(message_center_utils::GetSortedNotificationsWithOwnView());
}

void NotificationListView::Init(
    const std::vector<message_center::Notification*>& notifications) {
  for (auto* notification : notifications) {
    auto message_view_container = std::make_unique<MessageViewContainer>(
        CreateMessageView(*notification), /*list_view=*/this);
    message_view_container->set_disable_default_background(
        notification->custom_view_type() == kArcNotificationCustomViewType);
    // The insertion order for notifications is reversed.
    AddChildViewAt(std::move(message_view_container), children().size());
    MessageCenter::Get()->DisplayedNotification(
        notification->id(), message_center::DISPLAY_SOURCE_MESSAGE_CENTER);
  }

  // The latest notification will be expanded if auto-expanding is allowed, it
  // is not grouped and it has not been manually expanded or collapsed.
  if (!notifications.empty()) {
    auto* latest_mvc = AsMVC(children().front());
    if (!latest_mvc->IsGroupParent() &&
        !latest_mvc->message_view()->IsManuallyExpandedOrCollapsed()) {
      base::AutoReset<bool> auto_reset(&ignore_size_change_, true);
      latest_mvc->SetExpandedBySystem(
          latest_mvc->message_view()->IsAutoExpandingAllowed());
    }
  }

  UpdateBorders(/*force_update=*/true);
  UpdateBounds();
}

void NotificationListView::ClearAllWithAnimation() {
  if (state_ == State::CLEAR_ALL_STACKED ||
      state_ == State::CLEAR_ALL_VISIBLE) {
    return;
  }
  ResetBounds();

  UMA_HISTOGRAM_COUNTS_100("ChromeOS.SystemTray.NotificationsRemovedByClearAll",
                           children().size());

  // Record a ClosedByClearAll metric for each notification dismissed.
  for (views::View* child : children()) {
    auto* view = AsMVC(child);
    metrics_utils::LogClosedByClearAll(view->GetNotificationId());
  }

  {
    base::AutoReset<bool> auto_reset(&ignore_notification_remove_, true);
    message_center::MessageCenter::Get()->RemoveAllNotifications(
        true /* by_user */,
        message_center::MessageCenter::RemoveType::NON_PINNED);
  }

  state_ = State::CLEAR_ALL_STACKED;
  UpdateClearAllAnimation();
  if (state_ != State::IDLE) {
    StartAnimation();
  }
}

std::vector<message_center::Notification*>
NotificationListView::GetAllNotifications() const {
  std::vector<message_center::Notification*> notifications;
  for (views::View* view : children()) {
    // The view may be present in the view hierarchy, but deleted in the message
    // center.
    auto* notification = MessageCenter::Get()->FindVisibleNotificationById(
        AsMVC(view)->GetNotificationId());
    if (notification) {
      notifications.insert(notifications.begin(), notification);
    }
  }
  return notifications;
}

std::vector<std::string> NotificationListView::GetAllNotificationIds() const {
  std::vector<std::string> notifications;
  for (views::View* view : children()) {
    notifications.insert(notifications.begin(),
                         AsMVC(view)->GetNotificationId());
  }
  return notifications;
}

std::vector<raw_ptr<message_center::Notification, VectorExperimental>>
NotificationListView::GetNotificationsAboveY(int y_offset) const {
  std::vector<raw_ptr<message_center::Notification, VectorExperimental>>
      notifications;
  for (views::View* view : children()) {
    const int bottom_limit =
        view->bounds().y() + kNotificationIconStackThreshold;
    if (bottom_limit <= y_offset) {
      auto* notification = MessageCenter::Get()->FindVisibleNotificationById(
          AsMVC(view)->GetNotificationId());
      if (notification) {
        notifications.insert(notifications.begin(), notification);
      }
    }
  }
  return notifications;
}

std::vector<raw_ptr<message_center::Notification, VectorExperimental>>
NotificationListView::GetNotificationsBelowY(int y_offset) const {
  std::vector<raw_ptr<message_center::Notification, VectorExperimental>>
      notifications;
  for (views::View* view : children()) {
    const int bottom_limit =
        view->bounds().y() + kNotificationIconStackThreshold;
    if (bottom_limit >= y_offset) {
      auto* notification = MessageCenter::Get()->FindVisibleNotificationById(
          AsMVC(view)->GetNotificationId());
      if (notification) {
        notifications.push_back(notification);
      }
    }
  }
  return notifications;
}

std::vector<std::string> NotificationListView::GetNotificationIdsAboveY(
    int y_offset) const {
  std::vector<std::string> notifications;
  for (views::View* view : children()) {
    const int bottom_limit =
        view->bounds().y() + kNotificationIconStackThreshold;
    if (bottom_limit > y_offset) {
      continue;
    }
    notifications.insert(notifications.begin(),
                         AsMVC(view)->GetNotificationId());
  }
  return notifications;
}

std::vector<std::string> NotificationListView::GetNotificationIdsBelowY(
    int y_offset) const {
  std::vector<std::string> notifications;
  for (views::View* view : children()) {
    const int top_of_notification = view->bounds().y();
    if (top_of_notification < y_offset) {
      continue;
    }
    notifications.insert(notifications.begin(),
                         AsMVC(view)->GetNotificationId());
  }
  return notifications;
}

int NotificationListView::GetTotalNotificationCount() const {
  return static_cast<int>(children().size());
}

int NotificationListView::GetTotalPinnedNotificationCount() const {
  int count = 0;
  for (views::View* child : children()) {
    if (AsMVC(child)->IsPinned()) {
      count++;
    }
  }
  return count;
}

bool NotificationListView::IsAnimating() const {
  return animation_->is_animating();
}

double NotificationListView::GetCurrentAnimationValue() const {
  gfx::Tween::Type tween;
  switch (state_) {
    case State::IDLE:
      // No animations are used for State::IDLE.
      NOTREACHED();
    case State::CLEAR_ALL_STACKED:
    case State::MOVE_DOWN:
      tween = gfx::Tween::FAST_OUT_SLOW_IN;
      break;
    case State::CLEAR_ALL_VISIBLE:
      tween = gfx::Tween::EASE_IN;
      break;
    case State::EXPAND_OR_COLLAPSE:
      tween = gfx::Tween::FAST_OUT_SLOW_IN_3;
      break;
  }

  return gfx::Tween::CalculateValue(tween, animation_->GetCurrentValue());
}

bool NotificationListView::IsAnimatingExpandOrCollapseContainer(
    const views::View* view) const {
  if (!view || !expand_or_collapsing_container_) {
    return false;
  }

  DCHECK(views::IsViewClass<MessageViewContainer>(view))
      << view->GetClassName() << " is not a MessageViewContainer.";
  const MessageViewContainer* message_view_container = AsMVC(view);
  return message_view_container == expand_or_collapsing_container_;
}

void NotificationListView::OnNotificationSlidOut() {
  DeleteRemovedNotifications();

  // |message_center_view_| can be null in tests.
  if (message_center_view_) {
    message_center_view_->OnNotificationSlidOut();
  }

  state_ = State::MOVE_DOWN;
  UpdateBounds();
  StartAnimation();
}

void NotificationListView::ChildPreferredSizeChanged(views::View* child) {
  if (ignore_size_change_) {
    return;
  }

  auto* message_view_container = AsMVC(child);
  // Immediately complete the old expand/collapse animation. It will be snapped
  // to the target bounds when UpdateBounds() is called. If the other animations
  // are occurring, prefer them over expand/collapse.
  if (message_view_container->needs_bounds_animation() &&
      (state_ == State::IDLE || state_ == State::EXPAND_OR_COLLAPSE)) {
    if (animation_->is_animating()) {
      // Finish the previous expand animation instantly.
      animation_->End();
    }
    expand_or_collapsing_container_ = message_view_container;
    expand_or_collapsing_container_->set_needs_bounds_animation(false);
    UpdateBounds();
    state_ = State::EXPAND_OR_COLLAPSE;
    StartAnimation();
    return;
  }

  if (state_ == State::EXPAND_OR_COLLAPSE) {
    return;
  }

  ResetBounds();
}

void NotificationListView::PreferredSizeChanged() {
  views::View::PreferredSizeChanged();
  if (message_center_view_) {
    message_center_view_->ListPreferredSizeChanged();
  }
}

void NotificationListView::Layout(PassKey) {
  for (views::View* child : children()) {
    auto* view = AsMVC(child);
    if (state_ == State::IDLE) {
      view->SetBoundsRect(view->target_bounds());
      continue;
    }
    view->SetBoundsRect(gfx::Tween::RectValueBetween(GetCurrentAnimationValue(),
                                                     view->start_bounds(),
                                                     view->target_bounds()));
  }
}

gfx::Rect NotificationListView::GetNotificationBounds(
    const std::string& notification_id) const {
  const MessageViewContainer* child = nullptr;
  if (!notification_id.empty()) {
    child = GetNotificationById(notification_id);
  }
  return child ? child->bounds() : GetLastNotificationBounds();
}

gfx::Rect NotificationListView::GetLastNotificationBounds() const {
  return children().empty() ? gfx::Rect() : children().back()->bounds();
}

gfx::Rect NotificationListView::GetNotificationBoundsBelowY(
    int y_offset) const {
  const auto it = base::ranges::lower_bound(
      children(), y_offset, {},
      [](const views::View* v) { return v->bounds().bottom(); });
  return (it == children().cend()) ? gfx::Rect() : (*it)->bounds();
}

gfx::Size NotificationListView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (state_ == State::IDLE) {
    return gfx::Size(message_view_width_, target_height_);
  }

  return gfx::Size(message_view_width_,
                   gfx::Tween::IntValueBetween(GetCurrentAnimationValue(),
                                               start_height_, target_height_));
}

void NotificationListView::AnimateResize() {
  // TODO(crbug/1330026): Refactor NotificationListView animations to use
  // animation builder instead of the existing layout based animations.
}

message_center::MessageView*
NotificationListView::GetMessageViewForNotificationId(const std::string& id) {
  auto it = base::ranges::find(children(), id, [](views::View* child) {
    DCHECK(views::IsViewClass<MessageViewContainer>(child));
    return AsMVC(child)->message_view()->notification_id();
  });

  if (it == children().end()) {
    return nullptr;
  }
  return AsMVC(*it)->message_view();
}

void NotificationListView::ConvertNotificationViewToGroupedNotificationView(
    const std::string& ungrouped_notification_id,
    const std::string& new_grouped_notification_id) {
  auto* message_view =
      GetMessageViewForNotificationId(ungrouped_notification_id);
  if (message_view) {
    message_view->set_notification_id(new_grouped_notification_id);
  }
}

void NotificationListView::ConvertGroupedNotificationViewToNotificationView(
    const std::string& grouped_notification_id,
    const std::string& new_single_notification_id) {
  GetMessageViewForNotificationId(grouped_notification_id)
      ->set_notification_id(new_single_notification_id);
}

void NotificationListView::OnChildNotificationViewUpdated(
    const std::string& parent_notification_id,
    const std::string& child_notification_id) {
  auto* parent_view = GetMessageViewForNotificationId(parent_notification_id);
  if (!parent_view) {
    return;
  }

  // Update the child notification view with the updated notification.
  auto* child_view =
      parent_view->FindGroupNotificationView(child_notification_id);

  if (!child_view) {
    return;
  }

  auto* notification =
      MessageCenter::Get()->FindNotificationById(child_notification_id);
  static_cast<MessageView*>(child_view)->UpdateWithNotification(*notification);
  ResetBounds();
}

void NotificationListView::OnNotificationAdded(const std::string& id) {
  auto* notification = MessageCenter::Get()->FindVisibleNotificationById(id);
  if (!notification) {
    return;
  }

  // A group child notification should be added to its parent's message view.
  if (notification->group_child()) {
    return;
  }

  InterruptClearAll();

  // Collapse notifications that have not been manually expanded or collapsed.
  {
    base::AutoReset<bool> auto_reset(&ignore_size_change_, true);
    for (views::View* child : children()) {
      auto* mvc = AsMVC(child);
      if (!mvc->message_view()->IsManuallyExpandedOrCollapsed()) {
        mvc->message_view()->SetExpanded(false);
      }
    }
  }

  // We only need to update if we already have a message view associated with
  // the notification. This happens when we convert a single notification to a
  // group notification, ConvertNotificationViewToGroupedNotificationView()
  // changes the id of the single notification to the parent's.
  if (GetNotificationById(id)) {
    OnNotificationUpdated(id);
    return;
  }

  // Find the correct index to insert the new notification based on the sorted
  // order.
  auto child_views = children();
  size_t index_to_insert = child_views.size();
  for (size_t i = 0; i < child_views.size(); ++i) {
    MessageViewContainer* message_view =
        static_cast<MessageViewContainer*>(child_views[i]);
    auto* child_notification =
        MessageCenter::Get()->FindVisibleNotificationById(
            message_view->GetNotificationId());
    if (!child_notification) {
      break;
    }

    // The insertion order for notifications is reversed.
    if (!message_center_utils::CompareNotifications(child_notification,
                                                    notification)) {
      index_to_insert = i;
      break;
    }
  }

  auto view = CreateMessageView(*notification);
  view->SetExpanded(view->IsAutoExpandingAllowed());
  AddChildViewAt(std::make_unique<MessageViewContainer>(std::move(view),
                                                        /*list_view=*/this),
                 index_to_insert);
  UpdateBorders(/*force_update=*/false);
  ResetBounds();
}

void NotificationListView::OnNotificationRemoved(const std::string& id,
                                                 bool by_user) {
  if (ignore_notification_remove_) {
    return;
  }

  // The corresponding MessageView may have already been deleted after being
  // manually slid out.
  auto* child = GetNotificationById(id);
  if (!child) {
    return;
  }

  InterruptClearAll();
  ResetBounds();

  // `child` Can be deleted by either `InterruptClearAll` or `ResetBounds` since
  // both call `DeleteRemovedNotifications`. Therefore, we need to check if it
  // exists again. See https://b/315187548
  child = GetNotificationById(id);
  if (!child) {
    return;
  }

  child->set_is_removed(true);

  // If the MessageView is slid out, then do nothing here. The MOVE_DOWN
  // animation will be started in OnNotificationSlidOut().
  if (!child->is_slid_out()) {
    child->SlideOutAndClose();
  }
}

void NotificationListView::OnNotificationUpdated(const std::string& id) {
  auto* notification = MessageCenter::Get()->FindVisibleNotificationById(id);
  if (!notification) {
    return;
  }

  InterruptClearAll();

  MessageView* found_child = nullptr;
  for (views::View* child : children()) {
    auto* mvc = AsMVC(child);
    // First checks through the immediate children.
    if (mvc->GetNotificationId() == id) {
      found_child = mvc->message_view();
      break;
    }
  }

  // The corresponding MessageView may have been slid out and deleted, so just
  // ignore this update as the notification will soon be deleted.
  if (!found_child) {
    return;
  }

  int previous_height = found_child->GetPreferredSize().height();
  found_child->UpdateWithNotification(*notification);
  if (found_child->GetPreferredSize().height() != previous_height) {
    ResetBounds();
    state_ = State::MOVE_DOWN;
    StartAnimation();
  }
}

void NotificationListView::OnSlideStarted(const std::string& notification_id) {
  // When the swipe control for |notification_id| is shown, hide all other swipe
  // controls.
  for (views::View* child : children()) {
    auto* view = AsMVC(child);
    if (view->GetNotificationId() != notification_id) {
      view->CloseSwipeControl();
    }
  }
}

void NotificationListView::OnCloseButtonPressed(
    const std::string& notification_id) {
  metrics_utils::LogClosedByUser(notification_id, /*is_swipe=*/false,
                                 /*is_popup=*/false);
}

void NotificationListView::OnSettingsButtonPressed(
    const std::string& notification_id) {
  metrics_utils::LogSettingsShown(notification_id, /*is_slide_controls=*/false,
                                  /*is_popup=*/false);
}

void NotificationListView::OnSnoozeButtonPressed(
    const std::string& notification_id) {
  metrics_utils::LogSnoozed(notification_id, /*is_slide_controls=*/false,
                            /*is_popup=*/false);
}

void NotificationListView::AnimationEnded(const gfx::Animation* animation) {
  if (throughput_tracker_) {
    // Reset `throughput_tracker_` to reset animation metrics recording.
    throughput_tracker_->Stop();
    throughput_tracker_.reset();
  }

  // This is also called from AnimationCanceled().
  // TODO(crbug/1272104): Can we do better? If we are interrupting an animation,
  // this does not look good.
  animation_->SetCurrentValue(1.0);
  PreferredSizeChanged();

  switch (state_) {
    case State::IDLE:
    case State::EXPAND_OR_COLLAPSE:
      expand_or_collapsing_container_ = nullptr;
      [[fallthrough]];
    case State::MOVE_DOWN:
      state_ = State::IDLE;
      break;
    case State::CLEAR_ALL_STACKED:
    case State::CLEAR_ALL_VISIBLE:
      DeleteRemovedNotifications();
      UpdateClearAllAnimation();
      break;
  }

  UpdateBorders(/*force_update=*/false);

  if (state_ != State::IDLE) {
    StartAnimation();
  }
}

void NotificationListView::AnimationProgressed(
    const gfx::Animation* animation) {
  if (state_ == State::EXPAND_OR_COLLAPSE) {
    expand_or_collapsing_container_->TriggerPreferredSizeChangedForAnimation();
  }

  PreferredSizeChanged();
}

void NotificationListView::AnimationCanceled(const gfx::Animation* animation) {
  AnimationEnded(animation);
}

std::unique_ptr<MessageView> NotificationListView::CreateMessageView(
    const Notification& notification) {
  auto message_view =
      MessageViewFactory::Create(notification, /*shown_in_popup=*/false);
  ConfigureMessageView(message_view.get());
  return message_view;
}

void NotificationListView::ConfigureMessageView(
    message_center::MessageView* message_view) {
  // Setting grouped notifications as nested is handled in
  // `AshNotificationView`.
  auto* notification = MessageCenter::Get()->FindNotificationById(
      message_view->notification_id());
  // `notification` may not exist in tests.
  if (notification && !notification->group_child()) {
    message_view->SetIsNested();
  }
  message_view_multi_source_observation_.AddObservation(message_view);
  // `message_center_view_` may not exist in tests.
  if (message_center_view_) {
    message_center_view_->ConfigureMessageView(message_view);
  }
}

std::vector<raw_ptr<message_center::Notification, VectorExperimental>>
NotificationListView::GetStackedNotifications() const {
  return message_center_view_->GetStackedNotifications();
}

std::vector<std::string>
NotificationListView::GetNonVisibleNotificationIdsInViewHierarchy() const {
  return message_center_view_->GetNonVisibleNotificationIdsInViewHierarchy();
}

// static
const MessageViewContainer* NotificationListView::AsMVC(const views::View* v) {
  return static_cast<const MessageViewContainer*>(v);
}

// static
MessageViewContainer* NotificationListView::AsMVC(views::View* v) {
  return static_cast<MessageViewContainer*>(v);
}

const MessageViewContainer* NotificationListView::GetNotificationById(
    const std::string& id) const {
  const auto i = base::ranges::find(children(), id, [](const views::View* v) {
    return AsMVC(v)->GetNotificationId();
  });
  return (i == children().cend()) ? nullptr : AsMVC(*i);
}

MessageViewContainer* NotificationListView::GetNextRemovableNotification() {
  const auto i = base::ranges::find_if_not(
      base::Reversed(children()),
      [](const views::View* v) { return AsMVC(v)->IsPinned(); });
  return (i == children().rend()) ? nullptr : AsMVC(*i);
}

void NotificationListView::UpdateBorders(bool force_update) {
  // The top notification is drawn with rounded corners when the stacking bar
  // is not shown.
  bool is_top = state_ != State::MOVE_DOWN;
  for (views::View* child : children()) {
    AsMVC(child)->UpdateBorder(is_top, child == children().back(),
                               force_update);
    is_top = false;
  }
}

void NotificationListView::UpdateBounds() {
  int y = 0;
  for (views::View* child : children()) {
    auto* view = AsMVC(child);
    // Height is taken from preferred size, which is calculated based on the
    // tween and animation state when animations are occurring. So views which
    // are animating will provide the correct interpolated height here.
    const int height = view->CalculateHeight();
    const int direction = view->GetSlideDirection();

    if (y > 0) {
      y += kMessageListNotificationSpacing;
    }

    view->set_start_bounds(view->target_bounds());
    view->set_target_bounds(view->is_removed()
                                ? gfx::Rect(message_view_width_ * direction, y,
                                            message_view_width_, height)
                                : gfx::Rect(0, y, message_view_width_, height));
    y += height;
  }

  start_height_ = target_height_;
  target_height_ = y;
}

void NotificationListView::ResetBounds() {
  DeleteRemovedNotifications();
  UpdateBounds();

  state_ = State::IDLE;
  if (animation_->is_animating()) {
    animation_->End();
  } else {
    PreferredSizeChanged();
  }
}

void NotificationListView::InterruptClearAll() {
  if (state_ != State::CLEAR_ALL_STACKED &&
      state_ != State::CLEAR_ALL_VISIBLE) {
    return;
  }

  for (views::View* child : children()) {
    auto* view = AsMVC(child);
    if (!view->IsPinned()) {
      view->set_is_removed(true);
    }
  }

  DeleteRemovedNotifications();
}

void NotificationListView::DeleteRemovedNotifications() {
  views::View::Views removed_views;
  base::ranges::copy_if(
      children(), std::back_inserter(removed_views),
      [](const views::View* v) { return AsMVC(v)->is_removed(); });

  {
    base::AutoReset<bool> auto_reset(&is_deleting_removed_notifications_, true);
    for (views::View* view : removed_views) {
      message_view_multi_source_observation_.RemoveObservation(
          AsMVC(view)->message_view());
      RemoveChildViewT(view);
    }
  }

  UpdateBorders(/*force_update=*/false);
}

void NotificationListView::StartAnimation() {
  DCHECK_NE(state_, State::IDLE);

  base::TimeDelta animation_duration;

  switch (state_) {
    case State::IDLE:
      break;
    case State::MOVE_DOWN:
      SetupThroughputTrackerForAnimationSmoothness(
          GetWidget(), throughput_tracker_,
          kMoveDownAnimationSmoothnessHistogramName);
      animation_duration = kClosingAnimationDuration;
      break;
    case State::CLEAR_ALL_STACKED:
      SetupThroughputTrackerForAnimationSmoothness(
          GetWidget(), throughput_tracker_,
          kClearAllStackedAnimationSmoothnessHistogramName);
      animation_duration = kClearAllStackedAnimationDuration;
      break;
    case State::CLEAR_ALL_VISIBLE:
      SetupThroughputTrackerForAnimationSmoothness(
          GetWidget(), throughput_tracker_,
          kClearAllVisibleAnimationSmoothnessHistogramName);
      animation_duration = kClearAllVisibleAnimationDuration;
      break;
    case State::EXPAND_OR_COLLAPSE:
      SetupThroughputTrackerForAnimationSmoothness(
          GetWidget(), throughput_tracker_,
          kExpandOrCollapseAnimationSmoothnessHistogramName);
      DCHECK(expand_or_collapsing_container_);
      animation_duration =
          expand_or_collapsing_container_
              ? expand_or_collapsing_container_->GetBoundsAnimationDuration()
              : base::Milliseconds(
                    kLargeImageExpandAndCollapseAnimationDuration);
      break;
  }

  animation_->SetDuration(animation_duration);
  animation_->Start();
}

void NotificationListView::UpdateClearAllAnimation() {
  DCHECK(state_ == State::CLEAR_ALL_STACKED ||
         state_ == State::CLEAR_ALL_VISIBLE);

  auto* view = GetNextRemovableNotification();
  if (view) {
    view->set_is_removed(true);
  }

  if (state_ == State::CLEAR_ALL_STACKED) {
    const auto non_visible_notification_ids =
        GetNonVisibleNotificationIdsInViewHierarchy();
    if (view && non_visible_notification_ids.size() > 0) {
      // Immediately remove all notifications (if removable/not pinned) that are
      // outside of the scrollable window.
      for (const auto& id : non_visible_notification_ids) {
        auto* message_view_container = GetNotificationById(id);
        if (message_view_container && !message_view_container->IsPinned()) {
          message_view_container->set_is_removed(true);
        }
      }

      DeleteRemovedNotifications();
      UpdateBounds();
      start_height_ = target_height_;
      for (views::View* child : children()) {
        auto* child_view = AsMVC(child);
        child_view->set_start_bounds(child_view->target_bounds());
      }
      PreferredSizeChanged();
    } else {
      state_ = State::CLEAR_ALL_VISIBLE;
    }
  }

  if (state_ == State::CLEAR_ALL_VISIBLE) {
    UpdateBounds();

    if (view || start_height_ != target_height_) {
      state_ = State::CLEAR_ALL_VISIBLE;
    } else {
      state_ = State::IDLE;
    }
  }
}

BEGIN_METADATA(NotificationListView);
END_METADATA

}  // namespace ash
