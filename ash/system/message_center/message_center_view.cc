// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/message_center_view.h"

#include <list>
#include <map>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/message_center/ash_message_center_lock_screen_controller.h"
#include "ash/system/message_center/message_center_button_bar.h"
#include "ash/system/message_center/message_center_scroll_bar.h"
#include "ash/system/message_center/message_center_style.h"
#include "ash/system/message_center/notifier_settings_view.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/message_view_factory.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

using message_center::MessageCenter;
using message_center::MessageView;
using message_center::Notification;
using message_center::NotificationList;

namespace ash {

// static
const size_t MessageCenterView::kMaxVisibleNotifications = 100;

// static
bool MessageCenterView::disable_animation_for_testing = false;

namespace {

constexpr int kMinScrollViewHeight = 77;
constexpr int kEmptyViewHeight = 96;
constexpr gfx::Insets kEmptyViewPadding(0, 0, 24, 0);
constexpr int kScrollShadowOffsetY = -2;
constexpr int kScrollShadowBlur = 2;
constexpr SkColor kScrollShadowColor = SkColorSetA(SK_ColorBLACK, 0x24);

void SetViewHierarchyEnabled(views::View* view, bool enabled) {
  for (int i = 0; i < view->child_count(); i++)
    SetViewHierarchyEnabled(view->child_at(i), enabled);
  view->SetEnabled(enabled);
}

// Create a view that is shown when there are no notifications.
views::View* CreateEmptyNotificationView() {
  auto* view = new views::View;
  auto layout = std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical,
                                                   kEmptyViewPadding, 0);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_END);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  view->SetLayoutManager(std::move(layout));

  views::ImageView* icon = new views::ImageView();
  icon->SetImage(gfx::CreateVectorIcon(kNotificationCenterAllDoneIcon,
                                       message_center_style::kEmptyIconSize,
                                       message_center_style::kEmptyViewColor));
  icon->SetBorder(
      views::CreateEmptyBorder(message_center_style::kEmptyIconPadding));
  view->AddChildView(icon);

  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_NO_MESSAGES));
  label->SetEnabledColor(message_center_style::kEmptyViewColor);
  // "Roboto-Medium, 12sp" is specified in the mock.
  label->SetFontList(
      gfx::FontList().DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  label->SetSubpixelRenderingEnabled(false);
  view->AddChildView(label);

  view->SetPaintToLayer();
  view->layer()->SetFillsBoundsOpaquely(false);

  return view;
}

class MessageCenterScrollView : public views::ScrollView {
 public:
  MessageCenterScrollView(MessageCenterView* owner) : owner_(owner) {}
  ~MessageCenterScrollView() override = default;

 private:
  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kDialog;
    node_data->SetName(
        l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_FOOTER_TITLE));
  }

  // views::ScrollBarController:
  void ScrollToPosition(views::ScrollBar* source, int position) override {
    views::ScrollView::ScrollToPosition(source, position);
    owner_->UpdateScrollerShadowVisibility();
  }

  MessageCenterView* const owner_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterScrollView);
};

// A view that displays a shadow at the bottom when |scroller_| is bounded.
class ScrollShadowView : public views::View {
 public:
  ScrollShadowView(int max_scroll_view_height, int button_height)
      : max_scroll_view_height_(max_scroll_view_height),
        button_height_(button_height) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    set_can_process_events_within_subtree(false);
  }
  ~ScrollShadowView() override = default;

 protected:
  void PaintChildren(const views::PaintInfo& paint_info) override {
    views::View::PaintChildren(paint_info);

    if (height() != max_scroll_view_height_)
      return;

    // Draw a shadow at the bottom of the viewport when scrolled.
    DrawShadow(paint_info.context(),
               gfx::Rect(0, height(), width(), button_height_));
  }

 private:
  // Draws a drop shadow above |shadowed_area|.
  void DrawShadow(const ui::PaintContext& context,
                  const gfx::Rect& shadowed_area) {
    ui::PaintRecorder recorder(context, size());
    gfx::Canvas* canvas = recorder.canvas();
    cc::PaintFlags flags;
    gfx::ShadowValues shadow;
    shadow.emplace_back(gfx::Vector2d(0, kScrollShadowOffsetY),
                        kScrollShadowBlur, kScrollShadowColor);
    flags.setLooper(gfx::CreateShadowDrawLooper(shadow));
    flags.setAntiAlias(true);
    canvas->ClipRect(shadowed_area, SkClipOp::kDifference);
    canvas->DrawRect(shadowed_area, flags);
  }

  const int max_scroll_view_height_;
  const int button_height_;

  DISALLOW_COPY_AND_ASSIGN(ScrollShadowView);
};

}  // namespace

// MessageCenterView ///////////////////////////////////////////////////////////

MessageCenterView::MessageCenterView(MessageCenter* message_center,
                                     int max_height)
    : message_center_(message_center),
      settings_visible_(false),
      is_locked_(Shell::Get()->session_controller()->IsScreenLocked()) {
  if (is_locked_ && !AshMessageCenterLockScreenController::IsEnabled())
    mode_ = Mode::LOCKED;

  message_center_->AddObserver(this);
  set_notify_enter_exit_on_child(true);
  SetFocusBehavior(views::View::FocusBehavior::NEVER);

  button_bar_ = new MessageCenterButtonBar(this, message_center, is_locked_);
  button_bar_->SetCloseAllButtonEnabled(false);

  const int button_height = button_bar_->GetPreferredSize().height();
  const int max_scroll_view_height = max_height - button_height;

  scroller_shadow_ =
      new ScrollShadowView(max_scroll_view_height, button_height);

  scroller_ = new MessageCenterScrollView(this);
  // Need to set the transparent background explicitly, since ScrollView has
  // set the default opaque background color.
  scroller_->SetBackgroundColor(SK_ColorTRANSPARENT);
  scroller_->ClipHeightTo(kMinScrollViewHeight, max_scroll_view_height);
  scroller_->SetVerticalScrollBar(new MessageCenterScrollBar(nullptr));

  message_list_view_.reset(new MessageListView());
  message_list_view_->SetBorderPadding();
  message_list_view_->set_scroller(scroller_);
  message_list_view_->set_owned_by_client();
  message_list_view_->AddObserver(this);

  // We want to swap the contents of the scroll view between the empty list
  // view and the message list view, without constructing them afresh each
  // time.  So, since the scroll view deletes old contents each time you
  // set the contents (regardless of the |owned_by_client_| setting) we need
  // an intermediate view for the contents whose children we can swap in and
  // out.
  views::View* scroller_contents = new views::View();
  scroller_contents->SetLayoutManager(std::make_unique<views::FillLayout>());
  scroller_contents->AddChildView(message_list_view_.get());
  scroller_->SetContents(scroller_contents);

  settings_view_ = new NotifierSettingsView();
  settings_view_->SetBackground(
      views::CreateSolidBackground(message_center_style::kBackgroundColor));

  no_notifications_view_ = CreateEmptyNotificationView();

  scroller_->SetVisible(false);  // Because it has no notifications at first.
  settings_view_->SetVisible(mode_ == Mode::SETTINGS);
  no_notifications_view_->SetVisible(mode_ == Mode::NO_NOTIFICATIONS);

  AddChildView(no_notifications_view_);
  AddChildView(scroller_);
  AddChildView(scroller_shadow_);
  AddChildView(settings_view_);
  AddChildView(button_bar_);
}

MessageCenterView::~MessageCenterView() {
  message_list_view_->RemoveObserver(this);

  if (!is_closing_)
    message_center_->RemoveObserver(this);
}

void MessageCenterView::SetNotifications(
    const NotificationList::Notifications& notifications) {
  if (is_closing_)
    return;

  int index = 0;
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin();
       iter != notifications.end(); ++iter) {
    AddNotificationAt(*(*iter), index++);

    message_center_->DisplayedNotification(
        (*iter)->id(), message_center::DISPLAY_SOURCE_MESSAGE_CENTER);
    if (message_list_view_->GetNotificationCount() >=
        kMaxVisibleNotifications) {
      break;
    }
  }

  Update(false /* animate */);
  scroller_->RequestFocus();
}

void MessageCenterView::SetSettingsVisible(bool visible) {
  settings_visible_ = visible;
  Update(true /* animate */);
}

void MessageCenterView::ClearAllClosableNotifications() {
  if (is_closing_)
    return;

  is_clearing_all_notifications_ = true;
  UpdateButtonBarStatus();
  SetViewHierarchyEnabled(scroller_, false);
  message_list_view_->ClearAllClosableNotifications(
      scroller_->GetVisibleRect());
}

void MessageCenterView::OnLockStateChanged(bool locked) {
  is_locked_ = locked;
  Update(true /* animate */);
  // Refresh a11y information, because accessible name of the view changes.
  NotifyAccessibilityEvent(ax::mojom::Event::kAriaAttributeChanged, true);
}

void MessageCenterView::OnAllNotificationsCleared() {
  SetViewHierarchyEnabled(scroller_, true);
  button_bar_->SetCloseAllButtonEnabled(false);

  // The status of buttons will be updated after removing all notifications.

  // Action by user.
  message_center_->RemoveAllNotifications(
      true /* by_user */, MessageCenter::RemoveType::NON_PINNED);
  is_clearing_all_notifications_ = false;
}

size_t MessageCenterView::NumMessageViewsForTest() const {
  return message_list_view_->GetNotificationCount();
}

void MessageCenterView::OnSettingsChanged() {
  scroller_->InvalidateLayout();
  PreferredSizeChanged();
  Layout();
}

void MessageCenterView::SetIsClosing(bool is_closing) {
  is_closing_ = is_closing;
  if (is_closing)
    message_center_->RemoveObserver(this);
  else
    message_center_->AddObserver(this);
}

void MessageCenterView::UpdateScrollerShadowVisibility() {
  // |scroller_shadow_| is visible only if |scroller_| is not all scrolled.
  scroller_shadow_->SetVisible(scroller_->contents()->height() +
                                   scroller_->contents()->y() !=
                               scroller_shadow_->height());
}

void MessageCenterView::Layout() {
  if (is_closing_)
    return;

  int button_height = button_bar_->GetHeightForWidth(width());
  int settings_height =
      std::min(GetSettingsHeightForWidth(width()), height() - button_height);

  // In order to keep the fix for https://crbug.com/767805 working,
  // we have to always call SetBounds of scroller_.
  // TODO(tetsui): Fix the bug above without calling SetBounds, as SetBounds
  // invokes Layout() which is a heavy operation.
  scroller_->SetBounds(0, 0, width(), height() - button_height);
  scroller_shadow_->SetBounds(0, 0, width(), height() - button_height);
  if (settings_view_->visible()) {
    settings_view_->SetBounds(0, height() - settings_height, width(),
                              settings_height);
  }
  if (no_notifications_view_->visible())
    no_notifications_view_->SetBounds(0, 0, width(), kEmptyViewHeight);
  button_bar_->SetBounds(0, height() - button_height - settings_height, width(),
                         button_height);
  if (GetWidget())
    GetWidget()->GetRootView()->SchedulePaint();
}

gfx::Size MessageCenterView::CalculatePreferredSize() const {
  int width = 0;
  for (int i = 0; i < child_count(); ++i) {
    const views::View* child = child_at(i);
    if (child->visible())
      width = std::max(width, child->GetPreferredSize().width());
  }
  return gfx::Size(width, GetHeightForWidth(width));
}

int MessageCenterView::GetHeightForWidth(int width) const {
  if (settings_transition_animation_ &&
      settings_transition_animation_->is_animating()) {
    return button_bar_->GetHeightForWidth(width) +
           GetContentHeightDuringAnimation();
  }

  return button_bar_->GetHeightForWidth(width) +
         GetContentHeightForMode(mode_, width);
}

bool MessageCenterView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  // Do not rely on the default scroll event handler of ScrollView because
  // the scroll happens only when the focus is on the ScrollView. The
  // notification center will allow the scrolling even when the focus is on
  // the buttons.
  if (scroller_->bounds().Contains(event.location()))
    return scroller_->OnMouseWheel(event);
  return views::View::OnMouseWheel(event);
}

void MessageCenterView::OnMouseExited(const ui::MouseEvent& event) {
  if (is_closing_)
    return;

  message_list_view_->ResetRepositionSession();
  Update(true /* animate */);
}

void MessageCenterView::OnNotificationAdded(const std::string& id) {
  int index = 0;
  const NotificationList::Notifications& notifications =
      message_center_->GetVisibleNotifications();
  for (NotificationList::Notifications::const_iterator
           iter = notifications.begin();
       iter != notifications.end(); ++iter, ++index) {
    if ((*iter)->id() == id) {
      AddNotificationAt(*(*iter), index);
      break;
    }
    if (message_list_view_->GetNotificationCount() >=
        kMaxVisibleNotifications) {
      break;
    }
  }
  Update(true /* animate */);
}

void MessageCenterView::OnNotificationRemoved(const std::string& id,
                                              bool by_user) {
  auto view_pair = message_list_view_->GetNotificationById(id);
  MessageView* view = view_pair.second;
  if (!view)
    return;
  size_t index = view_pair.first;

  // We skip repositioning during clear-all anomation, since we don't need keep
  // positions.
  if (by_user && !is_clearing_all_notifications_) {
    gfx::RectF rect_f(view->x(), view->y(), view->width(), view->height());
    views::View::ConvertRectToTarget(view, message_list_view_.get(), &rect_f);
    message_list_view_->SetRepositionTarget(gfx::ToNearestRect(rect_f));
    // Moves the keyboard focus to the next notification if the removed
    // notification is focused so that the user can dismiss notifications
    // without re-focusing by tab key.
    if (view->IsCloseButtonFocused() || view->HasFocus()) {
      views::View* next_focused_view = nullptr;
      if (message_list_view_->GetNotificationCount() > index + 1)
        next_focused_view = message_list_view_->GetNotificationAt(index + 1);
      else if (index > 0)
        next_focused_view = message_list_view_->GetNotificationAt(index - 1);

      if (next_focused_view) {
        if (view->IsCloseButtonFocused()) {
          // Safe cast since all views in MessageListView are MessageViews.
          static_cast<MessageView*>(next_focused_view)
              ->RequestFocusOnCloseButton();
        } else {
          next_focused_view->RequestFocus();
        }
      }
    }
  }
  message_list_view_->RemoveNotification(view);
  Update(true /* animate */);
}

// This is a separate function so we can override it in tests.
bool MessageCenterView::SetRepositionTarget() {
  // Set the item on the mouse cursor as the reposition target so that it
  // should stick to the current position over the update.
  if (message_list_view_->IsMouseHovered()) {
    size_t count = message_list_view_->GetNotificationCount();
    for (size_t i = 0; i < count; ++i) {
      const views::View* hover_view = message_list_view_->GetNotificationAt(i);

      if (hover_view->IsMouseHovered()) {
        message_list_view_->SetRepositionTarget(hover_view->bounds());
        return true;
      }
    }
  }
  return false;
}

void MessageCenterView::OnNotificationUpdated(const std::string& id) {
  // If there is no reposition target anymore, make sure to reset the reposition
  // session.
  if (!SetRepositionTarget())
    message_list_view_->ResetRepositionSession();

  UpdateNotification(id);
}

void MessageCenterView::OnQuietModeChanged(bool is_quiet_mode) {
  settings_view_->SetQuietModeState(is_quiet_mode);
  button_bar_->SetQuietModeState(is_quiet_mode);
}

void MessageCenterView::AnimationEnded(const gfx::Animation* animation) {
  DCHECK_EQ(animation, settings_transition_animation_.get());

  if (source_view_) {
    source_view_->SetVisible(false);
  }
  if (target_view_)
    target_view_->SetVisible(true);
  if (settings_transition_animation_)
    NotifyAnimationState(false /* animating */);
  settings_transition_animation_.reset();
  PreferredSizeChanged();
  Layout();

  // We should update minimum fixed height based on new |scroller_| height.
  // This is required when switching between message list and settings panel.
  if (!scroller_->visible())
    message_list_view_->ResetRepositionSession();
}

void MessageCenterView::AnimationProgressed(const gfx::Animation* animation) {
  DCHECK_EQ(animation, settings_transition_animation_.get());
  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void MessageCenterView::AnimationCanceled(const gfx::Animation* animation) {
  DCHECK_EQ(animation, settings_transition_animation_.get());
  AnimationEnded(animation);
}

void MessageCenterView::OnViewPreferredSizeChanged(views::View* observed_view) {
  DCHECK_EQ(std::string(MessageView::kViewClassName),
            observed_view->GetClassName());
  UpdateNotification(
      static_cast<MessageView*>(observed_view)->notification_id());
}

void MessageCenterView::AddNotificationAt(const Notification& notification,
                                          int index) {
  MessageView* view = message_center::MessageViewFactory::Create(notification);
  // Not top-level.
  view->SetIsNested();
  view->AddObserver(this);
  view->set_scroller(scroller_);
  message_list_view_->AddNotificationAt(view, index);
}

void MessageCenterView::Update(bool animate) {
  bool no_message_views = (message_list_view_->GetNotificationCount() == 0);

  if (is_locked_ && !AshMessageCenterLockScreenController::IsEnabled())
    SetVisibilityMode(Mode::LOCKED, animate);
  else if (settings_visible_)
    SetVisibilityMode(Mode::SETTINGS, animate);
  else if (no_message_views)
    SetVisibilityMode(Mode::NO_NOTIFICATIONS, animate);
  else
    SetVisibilityMode(Mode::NOTIFICATIONS, animate);

  UpdateButtonBarStatus();

  if (scroller_->visible())
    scroller_->InvalidateLayout();
  PreferredSizeChanged();
  Layout();
}

void MessageCenterView::SetVisibilityMode(Mode mode, bool animate) {
  if (is_closing_)
    return;

  if (mode == mode_)
    return;

  switch (mode_) {
    case Mode::NOTIFICATIONS:
      source_view_ = scroller_;
      break;
    case Mode::SETTINGS:
      source_view_ = settings_view_;
      break;
    case Mode::LOCKED:
      source_view_ = nullptr;
      break;
    case Mode::NO_NOTIFICATIONS:
      source_view_ = no_notifications_view_;
      break;
  }

  switch (mode) {
    case Mode::NOTIFICATIONS:
      target_view_ = scroller_;
      break;
    case Mode::SETTINGS:
      target_view_ = settings_view_;
      break;
    case Mode::LOCKED:
      target_view_ = nullptr;
      break;
    case Mode::NO_NOTIFICATIONS:
      target_view_ = no_notifications_view_;
      break;
  }

  source_height_ = GetContentHeightForMode(mode_, width());
  target_height_ = GetContentHeightForMode(mode, width());

  mode_ = mode;

  int contents_max_height =
      max_height_ - button_bar_->GetPreferredSize().height();
  source_height_ = std::min(contents_max_height, source_height_);
  target_height_ = std::min(contents_max_height, target_height_);

  if (source_view_)
    source_view_->SetVisible(true);
  if (target_view_)
    target_view_->SetVisible(true);

  if (!animate || disable_animation_for_testing) {
    AnimationEnded(nullptr);
    return;
  }

  NotifyAnimationState(true /* animating */);

  settings_transition_animation_ = std::make_unique<gfx::SlideAnimation>(this);
  settings_transition_animation_->SetSlideDuration(
      message_center_style::kSettingsTransitionDurationMs);
  settings_transition_animation_->SetTweenType(gfx::Tween::EASE_IN_OUT);
  settings_transition_animation_->Show();
}

void MessageCenterView::UpdateButtonBarStatus() {
  // Disables all buttons during animation of cleaning of all notifications.
  if (is_clearing_all_notifications_) {
    button_bar_->SetSettingsAndQuietModeButtonsEnabled(false);
    button_bar_->SetCloseAllButtonEnabled(false);
    return;
  }

  button_bar_->SetBackArrowVisible(mode_ == Mode::SETTINGS);
  button_bar_->SetIsLocked(is_locked_);

  EnableCloseAllIfAppropriate();
}

void MessageCenterView::EnableCloseAllIfAppropriate() {
  if (mode_ == Mode::NOTIFICATIONS) {
    bool no_closable_views = true;
    size_t count = message_list_view_->GetNotificationCount();
    for (size_t i = 0; i < count; ++i) {
      if (message_list_view_->GetNotificationAt(i)->GetMode() ==
          MessageView::Mode::NORMAL) {
        no_closable_views = false;
        break;
      }
    }
    button_bar_->SetCloseAllButtonEnabled(!no_closable_views);
  } else {
    // Disable the close-all button since no notification is visible.
    button_bar_->SetCloseAllButtonEnabled(false);
  }
}

void MessageCenterView::SetNotificationViewForTest(MessageView* view) {
  message_list_view_->AddNotificationAt(view, 0);
}

void MessageCenterView::UpdateNotification(const std::string& id) {
  MessageView* view = message_list_view_->GetNotificationById(id).second;
  if (!view)
    return;
  Notification* notification = message_center_->FindVisibleNotificationById(id);
  if (notification) {
    int old_width = view->width();
    int old_height = view->height();
    MessageView::Mode old_mode = view->GetMode();
    message_list_view_->UpdateNotification(view, *notification);
    if (view->GetHeightForWidth(old_width) != old_height) {
      Update(true /* animate */);
    } else if (view->GetMode() != old_mode) {
      // Animate flag is false, since the pinned flag transition doesn't need
      // animation.
      Update(false /* animate */);
    }
  }

  // Notify accessibility that the contents have changed.
  view->NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged, false);
}

void MessageCenterView::NotifyAnimationState(bool animating) {
  size_t count = message_list_view_->GetNotificationCount();
  for (size_t i = 0; i < count; ++i) {
    MessageView* view = message_list_view_->GetNotificationAt(i);

    if (animating)
      view->OnContainerAnimationStarted();
    else
      view->OnContainerAnimationEnded();

    // Ensure that a notification is not removed or added during iteration.
    DCHECK_EQ(count, message_list_view_->GetNotificationCount());
  }
}

int MessageCenterView::GetSettingsHeightForWidth(int width) const {
  if (settings_transition_animation_ &&
      settings_transition_animation_->is_animating() &&
      (source_view_ == settings_view_ || target_view_ == settings_view_)) {
    return settings_transition_animation_->CurrentValueBetween(
        target_view_ == settings_view_ ? 0 : source_height_,
        source_view_ == settings_view_ ? 0 : target_height_);
  } else {
    return mode_ == Mode::SETTINGS ? settings_view_->GetHeightForWidth(width)
                                   : 0;
  }
}

int MessageCenterView::GetContentHeightDuringAnimation() const {
  DCHECK(settings_transition_animation_);
  int contents_height = settings_transition_animation_->CurrentValueBetween(
      target_view_ == settings_view_ ? 0 : source_height_,
      source_view_ == settings_view_ ? 0 : target_height_);
  if (target_view_ == settings_view_)
    contents_height = std::max(source_height_, contents_height);
  if (source_view_ == settings_view_)
    contents_height = std::max(target_height_, contents_height);
  return contents_height;
}

int MessageCenterView::GetContentHeightForMode(Mode mode, int width) const {
  switch (mode) {
    case Mode::NOTIFICATIONS:
      return scroller_->GetHeightForWidth(width);
    case Mode::SETTINGS:
      return settings_view_->GetHeightForWidth(width);
    case Mode::LOCKED:
      return 0;
    case Mode::NO_NOTIFICATIONS:
      return kEmptyViewHeight;
  }
}

}  // namespace ash
