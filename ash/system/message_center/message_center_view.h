// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_VIEW_H_
#define ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_VIEW_H_

#include <stddef.h>

#include "ash/ash_export.h"
#include "ash/session/session_observer.h"
#include "ash/system/message_center/message_list_view.h"
#include "base/macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/notification_list.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace gfx {
class SlideAnimation;
}  // namespace gfx

namespace message_center {

class MessageCenter;
class MessageView;

}  // namespace message_center

namespace ash {

class ArcNotificationContentViewTest;
class MessageCenterButtonBar;
class NotifierSettingsView;

// Container for all the top-level views in the notification center, such as the
// button bar, settings view, scrol view, and message list view.  Acts as a
// controller/delegate for the message list view, passing data back and forth to
// message center.
class ASH_EXPORT MessageCenterView
    : public views::View,
      public message_center::MessageCenterObserver,
      public SessionObserver,
      public MessageListView::Observer,
      public gfx::AnimationDelegate,
      public views::ViewObserver {
 public:
  MessageCenterView(message_center::MessageCenter* message_center,
                    int max_height);
  ~MessageCenterView() override;

  void SetNotifications(
      const message_center::NotificationList::Notifications& notifications);

  void ClearAllClosableNotifications();

  size_t NumMessageViewsForTest() const;

  void SetSettingsVisible(bool visible);
  void OnSettingsChanged();
  bool settings_visible() const { return settings_visible_; }

  void SetIsClosing(bool is_closing);

  void SetMaxHeight(int max_height) { max_height_ = max_height; }

  void UpdateScrollerShadowVisibility();

  static const size_t kMaxVisibleNotifications;

 protected:
  // Potentially sets the reposition target, and then returns whether or not it
  // was set.
  virtual bool SetRepositionTarget();

  // Overridden from views::View:
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // Overridden from MessageCenterObserver:
  void OnNotificationAdded(const std::string& id) override;
  void OnNotificationRemoved(const std::string& id, bool by_user) override;
  void OnNotificationUpdated(const std::string& id) override;
  void OnQuietModeChanged(bool is_quiet_mode) override;

  // Overridden from SessionObserver:
  void OnLockStateChanged(bool locked) override;

  // Overridden from MessageListView::Observer:
  void OnAllNotificationsCleared() override;

  // Overridden from gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  // Overridden from views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* observed_view) override;

 private:
  friend class ArcNotificationContentViewTest;
  friend class MessageCenterViewTest;

  // NOTIFICATIONS: Normal notification list (MessageListView) is shown.
  //   There should be at least one notification.
  // SETTINGS: Notifier settings (NotifierSettingsView) is shown.
  // LOCKED: The computer is in the lock screen. No content view is shown.
  // NO_NOTIFICATIONS: "All done" message (EmptyNotifcationView) is shown.
  //   There should be no notification.
  enum class Mode { NOTIFICATIONS, SETTINGS, LOCKED, NO_NOTIFICATIONS };

  static bool disable_animation_for_testing;

  void AddNotificationAt(const message_center::Notification& notification,
                         int index);
  void Update(bool animate);
  void SetVisibilityMode(Mode mode, bool animate);
  void UpdateButtonBarStatus();
  void EnableCloseAllIfAppropriate();
  void SetNotificationViewForTest(message_center::MessageView* view);
  void UpdateNotification(const std::string& notification_id);
  void NotifyAnimationState(bool animating);

  // There are three patterns for animation.
  // - Only MessageCenterView height changes.
  // - Both MessageCenterview and NotifierSettingsView moves at same velocity.
  // - Only NotifierSettingsView moves.
  // Thus, these two methods are needed.
  int GetSettingsHeightForWidth(int width) const;
  int GetContentHeightDuringAnimation() const;

  // Returns the height for the given |width| of the view correspond to |mode|
  // e.g. |settings_view_|.
  int GetContentHeightForMode(Mode mode, int width) const;

  message_center::MessageCenter* message_center_;

  // Child views.
  views::ScrollView* scroller_ = nullptr;
  std::unique_ptr<MessageListView> message_list_view_;
  views::View* scroller_shadow_ = nullptr;
  NotifierSettingsView* settings_view_ = nullptr;
  views::View* no_notifications_view_ = nullptr;
  MessageCenterButtonBar* button_bar_ = nullptr;

  // Data for transition animation between settings view and message list.
  bool settings_visible_;

  // Animation managing transition between message center and settings (and vice
  // versa).
  std::unique_ptr<gfx::SlideAnimation> settings_transition_animation_;

  // Helper data to keep track of the transition between settings and
  // message center views.
  views::View* source_view_ = nullptr;
  int source_height_ = 0;
  views::View* target_view_ = nullptr;
  int target_height_ = 0;

  // Maximum height set for the MessageCenterBubble by SetMaxHeight.
  int max_height_ = 0;

  // True when the widget is closing so that further operations should be
  // ignored.
  bool is_closing_ = false;

  bool is_clearing_all_notifications_ = false;
  bool is_locked_;

  // Current view mode. During animation, it is the target mode.
  Mode mode_ = Mode::NO_NOTIFICATIONS;

  ScopedSessionObserver session_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(MessageCenterView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_VIEW_H_
