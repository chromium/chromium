// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/unified_message_list_view.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/message_center/message_center_style.h"
#include "ash/system/message_center/notification_swipe_control_view.h"
#include "ash/system/message_center/unified_message_center_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/auto_reset.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_view_factory.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

using message_center::Notification;
using message_center::MessageCenter;
using message_center::MessageView;

namespace ash {

namespace {

constexpr base::TimeDelta kClosingAnimationDuration =
    base::TimeDelta::FromMilliseconds(320);
constexpr base::TimeDelta kClearAllStackedAnimationDuration =
    base::TimeDelta::FromMilliseconds(40);
constexpr base::TimeDelta kClearAllVisibleAnimationDuration =
    base::TimeDelta::FromMilliseconds(160);

// Comparator function for sorting the notifications in the order that they are
// displayed in the UnifiedMessageListView.
// Currently the ordering rule is very simple (subject to change):
//     1. All pinned notifications are displayed first.
//     2. Otherwise, display in order of most recent timestamp.
bool CompareNotifications(message_center::Notification* n1,
                          message_center::Notification* n2) {
  if (n1->pinned() && !n2->pinned())
    return true;
  if (!n1->pinned() && n2->pinned())
    return false;
  return message_center::CompareTimestampSerial()(n1, n2);
}

}  // namespace

// The background of the UnifiedMessageListView, which has a strait top and a
// rounded bottom.
class UnifiedMessageListView::Background : public views::Background {
 public:
  Background() = default;
  ~Background() override = default;

  // views::Background:
  void Paint(gfx::Canvas* canvas, View* view) const override {
    gfx::Rect bounds = view->GetLocalBounds();
    SkPath background_path;
    SkScalar radius = SkIntToScalar(kUnifiedTrayCornerRadius);
    SkScalar radii[8] = {0, 0, 0, 0, radius, radius, radius, radius};
    background_path.addRoundRect(gfx::RectToSkRect(bounds), radii);

    cc::PaintFlags flags;
    flags.setColor(message_center_style::kSwipeControlBackgroundColor);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);
    canvas->DrawPath(background_path, flags);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Background);
};

// Container view of notification and swipe control.
// All children of UnifiedMessageListView should be MessageViewContainer.
class UnifiedMessageListView::MessageViewContainer
    : public views::View,
      public MessageView::SlideObserver {
 public:
  MessageViewContainer(MessageView* message_view,
                       UnifiedMessageListView* list_view)
      : message_view_(message_view),
        list_view_(list_view),
        control_view_(new NotificationSwipeControlView(message_view)) {
    message_view_->AddSlideObserver(this);

    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddChildView(control_view_);
    AddChildView(message_view_);
  }

  ~MessageViewContainer() override { message_view_->RemoveSlideObserver(this); }

  // Update the border and background corners based on if the notification is
  // at the top or the bottom.
  void UpdateBorder(bool is_top, bool is_bottom) {
    message_view_->SetBorder(
        is_bottom ? views::NullBorder()
                  : views::CreateSolidSidedBorder(
                        0, 0, kUnifiedNotificationSeparatorThickness, 0,
                        AshColorProvider::Get()->GetContentLayerColor(
                            AshColorProvider::ContentLayerType::kSeparator,
                            AshColorProvider::AshColorMode::kLight)));
    const int top_radius = is_top ? kUnifiedTrayCornerRadius : 0;
    const int bottom_radius = is_bottom ? kUnifiedTrayCornerRadius : 0;
    message_view_->UpdateCornerRadius(top_radius, bottom_radius);
    control_view_->UpdateCornerRadius(top_radius, bottom_radius);
  }

  // Collapses the notification if its state haven't changed manually by a user.
  void Collapse() {
    if (!message_view_->IsManuallyExpandedOrCollapsed())
      message_view_->SetExpanded(false);
  }

  // Check if the notification is manually expanded / collapsed before and
  // restores the state.
  void LoadExpandedState(UnifiedSystemTrayModel* model, bool is_latest) {
    base::Optional<bool> manually_expanded =
        model->GetNotificationExpanded(GetNotificationId());
    if (manually_expanded.has_value()) {
      message_view_->SetExpanded(manually_expanded.value());
      message_view_->SetManuallyExpandedOrCollapsed(true);
    } else {
      // Expand the latest notification, and collapse all other notifications.
      message_view_->SetExpanded(is_latest &&
                                 message_view_->IsAutoExpandingAllowed());
    }
  }

  // Stores if the notification is manually expanded or collapsed so that we can
  // restore that when UnifiedSystemTray is reopened.
  void StoreExpandedState(UnifiedSystemTrayModel* model) {
    if (message_view_->IsManuallyExpandedOrCollapsed()) {
      model->SetNotificationExpanded(GetNotificationId(),
                                     message_view_->IsExpanded());
    }
  }

  void SlideOutAndClose() {
    message_view_->SlideOutAndClose(1 /* direction */);
  }

  std::string GetNotificationId() const {
    return message_view_->notification_id();
  }

  void UpdateWithNotification(const Notification& notification) {
    message_view_->UpdateWithNotification(notification);
  }

  void CloseSwipeControl() { message_view_->CloseSwipeControl(); }

  // Returns if the notification is pinned i.e. can be removed manually.
  bool IsPinned() const {
    return message_view_->GetMode() == MessageView::Mode::PINNED;
  }

  // Returns the direction that the notification is swiped out. If swiped to the
  // left, it returns -1 and if sipwed to the right, it returns 1. By default
  // (i.e. the notification is removed but not by touch gesture), it returns 1.
  int GetSlideDirection() const {
    return message_view_->GetSlideAmount() < 0 ? -1 : 1;
  }

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  const char* GetClassName() const override { return "UnifiedMessageListView"; }

  // MessageView::SlideObserver:
  void OnSlideChanged(const std::string& notification_id) override {
    control_view_->UpdateButtonsVisibility();
  }

  void OnSlideOut(const std::string& notification_id) override {
    is_slid_out_ = true;
    set_is_removed();
    list_view_->OnNotificationSlidOut();
  }

  gfx::Rect start_bounds() const { return start_bounds_; }
  gfx::Rect ideal_bounds() const { return ideal_bounds_; }
  bool is_removed() const { return is_removed_; }

  void set_start_bounds(const gfx::Rect& start_bounds) {
    start_bounds_ = start_bounds;
  }

  void set_ideal_bounds(const gfx::Rect& ideal_bounds) {
    ideal_bounds_ = ideal_bounds;
  }

  void set_is_removed() { is_removed_ = true; }

  bool is_slid_out() { return is_slid_out_; }

 private:
  // The bounds that the container starts animating from. If not animating, it's
  // ignored.
  gfx::Rect start_bounds_;

  // The final bounds of the container. If not animating, it's same as the
  // actual bounds().
  gfx::Rect ideal_bounds_;

  // True when the notification is removed and during slide out animation.
  bool is_removed_ = false;

  // True if the notification is slid out completely.
  bool is_slid_out_ = false;

  MessageView* const message_view_;
  UnifiedMessageListView* const list_view_;
  NotificationSwipeControlView* const control_view_;

  DISALLOW_COPY_AND_ASSIGN(MessageViewContainer);
};

UnifiedMessageListView::UnifiedMessageListView(
    UnifiedMessageCenterView* message_center_view,
    UnifiedSystemTrayModel* model)
    : views::AnimationDelegateViews(this),
      message_center_view_(message_center_view),
      model_(model),
      animation_(std::make_unique<gfx::LinearAnimation>(this)) {
  MessageCenter::Get()->AddObserver(this);
  animation_->SetCurrentValue(1.0);
  SetBackground(std::unique_ptr<views::Background>(
      new UnifiedMessageListView::Background()));
}

UnifiedMessageListView::~UnifiedMessageListView() {
  // The MessageCenter may be destroyed already during shutdown. See
  // crbug.com/946153.
  if (MessageCenter::Get())
    MessageCenter::Get()->RemoveObserver(this);

  model_->ClearNotificationChanges();
  for (auto* view : children())
    AsMVC(view)->StoreExpandedState(model_);
}

void UnifiedMessageListView::Init() {
  bool is_latest = true;
  for (auto* notification : GetSortedVisibleNotifications()) {
    auto* view =
        new MessageViewContainer(CreateMessageView(*notification), this);
    view->LoadExpandedState(model_, is_latest);
    AddChildViewAt(view, 0);
    MessageCenter::Get()->DisplayedNotification(
        notification->id(), message_center::DISPLAY_SOURCE_MESSAGE_CENTER);
    is_latest = false;
  }
  UpdateBorders();
  UpdateBounds();
}

void UnifiedMessageListView::ClearAllWithAnimation() {
  if (state_ == State::CLEAR_ALL_STACKED || state_ == State::CLEAR_ALL_VISIBLE)
    return;
  ResetBounds();

  {
    base::AutoReset<bool> auto_reset(&ignore_notification_remove_, true);
    message_center::MessageCenter::Get()->RemoveAllNotifications(
        true /* by_user */,
        message_center::MessageCenter::RemoveType::NON_PINNED);
  }

  state_ = State::CLEAR_ALL_STACKED;
  UpdateClearAllAnimation();
  if (state_ != State::IDLE)
    StartAnimation();
}

std::vector<Notification*> UnifiedMessageListView::GetNotificationsAboveY(
    int y_offset) const {
  std::vector<Notification*> notifications;
  for (views::View* view : children()) {
    int bottom_limit =
        features::IsUnifiedMessageCenterRefactorEnabled()
            ? view->bounds().y() + kNotificationIconStackThreshold
            : view->bounds().bottom();
    if (bottom_limit <= y_offset) {
      Notification* notification =
          MessageCenter::Get()->FindVisibleNotificationById(
              AsMVC(view)->GetNotificationId());
      if (notification)
        notifications.insert(notifications.begin(), notification);
    }
  }
  return notifications;
}

int UnifiedMessageListView::GetTotalNotificationCount() const {
  return int{children().size()};
}

bool UnifiedMessageListView::IsAnimating() const {
  return animation_->is_animating();
}

void UnifiedMessageListView::ChildPreferredSizeChanged(views::View* child) {
  if (ignore_size_change_)
    return;
  ResetBounds();
}

void UnifiedMessageListView::PreferredSizeChanged() {
  views::View::PreferredSizeChanged();
  if (message_center_view_)
    message_center_view_->ListPreferredSizeChanged();
}

void UnifiedMessageListView::Layout() {
  for (auto* child : children()) {
    auto* view = AsMVC(child);
    view->SetBoundsRect(gfx::Tween::RectValueBetween(
        GetCurrentValue(), view->start_bounds(), view->ideal_bounds()));
  }
}

gfx::Rect UnifiedMessageListView::GetNotificationBounds(
    const std::string& notification_id) const {
  const MessageViewContainer* child = nullptr;
  if (!notification_id.empty())
    child = GetNotificationById(notification_id);
  return child ? child->bounds() : GetLastNotificationBounds();
}

gfx::Rect UnifiedMessageListView::GetLastNotificationBounds() const {
  return children().empty() ? gfx::Rect() : children().back()->bounds();
}

gfx::Rect UnifiedMessageListView::GetNotificationBoundsBelowY(
    int y_offset) const {
  const auto it = std::find_if(children().cbegin(), children().cend(),
                               [y_offset](const views::View* v) {
                                 return v->bounds().bottom() >= y_offset;
                               });
  return (it == children().cend()) ? gfx::Rect() : (*it)->bounds();
}

gfx::Size UnifiedMessageListView::CalculatePreferredSize() const {
  return gfx::Size(kTrayMenuWidth,
                   gfx::Tween::IntValueBetween(GetCurrentValue(), start_height_,
                                               ideal_height_));
}

const char* UnifiedMessageListView::GetClassName() const {
  return "UnifiedMessageListView";
}

void UnifiedMessageListView::OnNotificationAdded(const std::string& id) {
  auto* notification = MessageCenter::Get()->FindVisibleNotificationById(id);
  if (!notification)
    return;

  InterruptClearAll();

  // Collapse all notifications before adding new one.
  CollapseAllNotifications();

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
    if (!child_notification)
      break;

    if (!CompareNotifications(notification, child_notification)) {
      index_to_insert = i;
      break;
    }
  }

  auto* view = CreateMessageView(*notification);
  view->SetExpanded(view->IsAutoExpandingAllowed());
  AddChildViewAt(new MessageViewContainer(view, this), index_to_insert);
  UpdateBorders();
  ResetBounds();
}

void UnifiedMessageListView::OnNotificationRemoved(const std::string& id,
                                                   bool by_user) {
  if (ignore_notification_remove_)
    return;

  // The corresponding MessageView may have already been deleted after being
  // manually slid out.
  auto* child = GetNotificationById(id);
  if (!child)
    return;

  InterruptClearAll();
  ResetBounds();

  child->set_is_removed();

  // If the MessageView is slid out, then do nothing here. The MOVE_DOWN
  // animation will be started in OnNotificationSlidOut().
  if (!child->is_slid_out())
    child->SlideOutAndClose();
}

void UnifiedMessageListView::OnNotificationSlidOut() {
  DeleteRemovedNotifications();

  // |message_center_view_| can be null in tests.
  if (message_center_view_)
    message_center_view_->OnNotificationSlidOut();

  state_ = State::MOVE_DOWN;
  UpdateBounds();
  StartAnimation();
}

void UnifiedMessageListView::OnNotificationUpdated(const std::string& id) {
  auto* notification = MessageCenter::Get()->FindVisibleNotificationById(id);
  if (!notification)
    return;

  InterruptClearAll();

  // The corresponding MessageView may have been slid out and deleted, so just
  // ignore this update as the notification will soon be deleted.
  auto* child = GetNotificationById(id);
  if (!child)
    return;

  child->UpdateWithNotification(*notification);
  ResetBounds();
}

void UnifiedMessageListView::OnSlideStarted(
    const std::string& notification_id) {
  // When the swipe control for |notification_id| is shown, hide all other swipe
  // controls.
  for (auto* child : children()) {
    auto* view = AsMVC(child);
    if (view->GetNotificationId() != notification_id)
      view->CloseSwipeControl();
  }
}

void UnifiedMessageListView::AnimationEnded(const gfx::Animation* animation) {
  // This is also called from AnimationCanceled().
  animation_->SetCurrentValue(1.0);
  PreferredSizeChanged();

  if (state_ == State::MOVE_DOWN) {
    state_ = State::IDLE;
  } else if (state_ == State::CLEAR_ALL_STACKED ||
             state_ == State::CLEAR_ALL_VISIBLE) {
    DeleteRemovedNotifications();
    UpdateClearAllAnimation();
  }

  UpdateBorders();

  if (state_ != State::IDLE)
    StartAnimation();
}

void UnifiedMessageListView::AnimationProgressed(
    const gfx::Animation* animation) {
  PreferredSizeChanged();
}

void UnifiedMessageListView::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

MessageView* UnifiedMessageListView::CreateMessageView(
    const Notification& notification) {
  auto* view = message_center::MessageViewFactory::Create(notification);
  view->SetIsNested();
  view->AddSlideObserver(this);
  message_center_view_->ConfigureMessageView(view);
  return view;
}

std::vector<message_center::Notification*>
UnifiedMessageListView::GetStackedNotifications() const {
  return message_center_view_->GetStackedNotifications();
}

// static
const UnifiedMessageListView::MessageViewContainer*
UnifiedMessageListView::AsMVC(const views::View* v) {
  return static_cast<const MessageViewContainer*>(v);
}

// static
UnifiedMessageListView::MessageViewContainer* UnifiedMessageListView::AsMVC(
    views::View* v) {
  return static_cast<MessageViewContainer*>(v);
}

const UnifiedMessageListView::MessageViewContainer*
UnifiedMessageListView::GetNotificationById(const std::string& id) const {
  const auto i = std::find_if(
      children().cbegin(), children().cend(),
      [id](const auto* v) { return AsMVC(v)->GetNotificationId() == id; });
  return (i == children().cend()) ? nullptr : AsMVC(*i);
}

UnifiedMessageListView::MessageViewContainer*
UnifiedMessageListView::GetNextRemovableNotification() {
  const auto i =
      std::find_if(children().cbegin(), children().cend(),
                   [](const auto* v) { return !AsMVC(v)->IsPinned(); });
  return (i == children().cend()) ? nullptr : AsMVC(*i);
}

void UnifiedMessageListView::CollapseAllNotifications() {
  base::AutoReset<bool> auto_reset(&ignore_size_change_, true);
  for (auto* child : children())
    AsMVC(child)->Collapse();
}

void UnifiedMessageListView::UpdateBorders() {
  // The top notification is drawn with rounded corners when the stacking bar is
  // not shown.
  bool is_top = children().size() == 1 && state_ != State::MOVE_DOWN;
  for (auto* child : children()) {
    AsMVC(child)->UpdateBorder(is_top, child == children().back());
    is_top = false;
  }
}

void UnifiedMessageListView::UpdateBounds() {
  int y = 0;
  for (auto* child : children()) {
    auto* view = AsMVC(child);
    const int height = view->GetHeightForWidth(kTrayMenuWidth);
    const int direction = view->GetSlideDirection();
    view->set_start_bounds(view->ideal_bounds());
    view->set_ideal_bounds(
        view->is_removed()
            ? gfx::Rect(kTrayMenuWidth * direction, y, kTrayMenuWidth, height)
            : gfx::Rect(0, y, kTrayMenuWidth, height));
    y += height;
  }

  start_height_ = ideal_height_;
  ideal_height_ = y;
}

void UnifiedMessageListView::ResetBounds() {
  DeleteRemovedNotifications();
  UpdateBounds();

  state_ = State::IDLE;
  if (animation_->is_animating())
    animation_->End();
  else
    PreferredSizeChanged();
}

void UnifiedMessageListView::InterruptClearAll() {
  if (state_ != State::CLEAR_ALL_STACKED && state_ != State::CLEAR_ALL_VISIBLE)
    return;

  for (auto* child : children()) {
    auto* view = AsMVC(child);
    if (!view->IsPinned())
      view->set_is_removed();
  }

  DeleteRemovedNotifications();
}

void UnifiedMessageListView::DeleteRemovedNotifications() {
  views::View::Views removed_views;
  std::copy_if(children().cbegin(), children().cend(),
               std::back_inserter(removed_views),
               [](const auto* v) { return AsMVC(v)->is_removed(); });

  {
    base::AutoReset<bool> auto_reset(&is_deleting_removed_notifications_, true);
    for (auto* view : removed_views) {
      model_->RemoveNotificationExpanded(AsMVC(view)->GetNotificationId());
      delete view;
    }
  }

  UpdateBorders();
}

void UnifiedMessageListView::StartAnimation() {
  DCHECK_NE(state_, State::IDLE);

  switch (state_) {
    case State::IDLE:
      break;
    case State::MOVE_DOWN:
      animation_->SetDuration(kClosingAnimationDuration);
      animation_->Start();
      break;
    case State::CLEAR_ALL_STACKED:
      animation_->SetDuration(kClearAllStackedAnimationDuration);
      animation_->Start();
      break;
    case State::CLEAR_ALL_VISIBLE:
      animation_->SetDuration(kClearAllVisibleAnimationDuration);
      animation_->Start();
      break;
  }
}

void UnifiedMessageListView::UpdateClearAllAnimation() {
  DCHECK(state_ == State::CLEAR_ALL_STACKED ||
         state_ == State::CLEAR_ALL_VISIBLE);

  auto* view = GetNextRemovableNotification();
  if (view)
    view->set_is_removed();

  if (state_ == State::CLEAR_ALL_STACKED) {
    if (view && GetStackedNotifications().size() > 0) {
      DeleteRemovedNotifications();
      UpdateBounds();
      start_height_ = ideal_height_;
      for (auto* child : children()) {
        auto* view = AsMVC(child);
        view->set_start_bounds(view->ideal_bounds());
      }

      PreferredSizeChanged();

      state_ = State::CLEAR_ALL_STACKED;
    } else {
      state_ = State::CLEAR_ALL_VISIBLE;
    }
  }

  if (state_ == State::CLEAR_ALL_VISIBLE) {
    UpdateBounds();

    if (view || start_height_ != ideal_height_)
      state_ = State::CLEAR_ALL_VISIBLE;
    else
      state_ = State::IDLE;
  }
}

double UnifiedMessageListView::GetCurrentValue() const {
  return gfx::Tween::CalculateValue(state_ == State::CLEAR_ALL_VISIBLE
                                        ? gfx::Tween::EASE_IN
                                        : gfx::Tween::FAST_OUT_SLOW_IN,
                                    animation_->GetCurrentValue());
}

std::vector<message_center::Notification*>
UnifiedMessageListView::GetSortedVisibleNotifications() const {
  auto visible_notifications = MessageCenter::Get()->GetVisibleNotifications();
  std::vector<Notification*> sorted_notifications;
  std::copy(visible_notifications.begin(), visible_notifications.end(),
            std::back_inserter(sorted_notifications));
  std::sort(sorted_notifications.begin(), sorted_notifications.end(),
            CompareNotifications);
  return sorted_notifications;
}

}  // namespace ash
