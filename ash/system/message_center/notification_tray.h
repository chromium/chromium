// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_NOTIFICATION_TRAY_H_
#define ASH_SYSTEM_MESSAGE_CENTER_NOTIFICATION_TRAY_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/login_status.h"
#include "ash/system/message_center/message_center_ui_delegate.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/animation/animation_container.h"

namespace aura {
class Window;
}

namespace message_center {
class MessageCenter;
class MessagePopupCollection;
}  // namespace message_center

namespace ash {
class AshPopupAlignmentDelegate;
class MessageCenterBubble;
class MessageCenterUiController;
class NotificationBubbleWrapper;
class NotificationTrayImageSubview;
class NotificationTrayLabelSubview;

// Status area tray for showing System, Chrome App, Web and ARC++ app
// notifications. This hosts a MessageCenter class which manages the
// notification list. This class contains the Ash specific tray implementation.
class ASH_EXPORT NotificationTray
    : public TrayBackgroundView,
      public MessageCenterUiDelegate,
      public base::SupportsWeakPtr<NotificationTray> {
 public:
  NotificationTray(Shelf* shelf, aura::Window* status_area_window);
  ~NotificationTray() override;

  static void DisableAnimationsForTest(bool disable);

  // Sets the height of the system tray bubble (or legacy notification bubble)
  // from the edge of the work area so that the notification popups don't
  // overlap with the tray. Pass 0 if no bubble is shown.
  void SetTrayBubbleHeight(int height);

  // Returns the current tray bubble height or 0 if there is no bubble.
  int tray_bubble_height_for_test() const;

  // Returns true if the message center bubble is visible.
  bool IsMessageCenterVisible() const;

  // Overridden from TrayBackgroundView.
  void UpdateAfterShelfAlignmentChange() override;
  void UpdateAfterRootWindowBoundsChange(const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds) override;
  void AnchorUpdated() override;
  base::string16 GetAccessibleNameForTray() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;
  bool PerformAction(const ui::Event& event) override;
  void CloseBubble() override;
  void ShowBubble(bool show_by_click) override;
  TrayBubbleView* GetBubbleView() override;

  // Overridden from TrayBubbleView::Delegate.
  void BubbleViewDestroyed() override;
  base::string16 GetAccessibleNameForBubble() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // Overridden from message_center::UiDelegate.
  void OnMessageCenterContentsChanged() override;
  bool ShowMessageCenter(bool show_by_click) override;
  void HideMessageCenter() override;
  bool ShowPopups() override;
  void HidePopups() override;

  // Activates the notification tray bubble.
  void ActivateBubble();

  message_center::MessageCenter* message_center() const;

 private:
  friend class NotificationTrayTest;

  FRIEND_TEST_ALL_PREFIXES(NotificationTrayTest, Notifications);
  FRIEND_TEST_ALL_PREFIXES(NotificationTrayTest, NotificationPopupBubble);
  FRIEND_TEST_ALL_PREFIXES(NotificationTrayTest,
                           ManyMessageCenterNotifications);
  FRIEND_TEST_ALL_PREFIXES(NotificationTrayTest, ManyPopupNotifications);
  FRIEND_TEST_ALL_PREFIXES(NotificationTrayTest, PopupShownOnBothDisplays);
  FRIEND_TEST_ALL_PREFIXES(NotificationTrayTest, PopupAndSystemTray);
  FRIEND_TEST_ALL_PREFIXES(NotificationTrayTest, PopupAndAutoHideShelf);
  FRIEND_TEST_ALL_PREFIXES(NotificationTrayTest, VisibleSmallIcon);
  FRIEND_TEST_ALL_PREFIXES(NotificationTrayTest, QuietModeIcon);
  FRIEND_TEST_ALL_PREFIXES(NotificationTrayTest, CloseOnActivation);

  void UpdateTrayContent();

  // Queries login status and the status area widget to determine visibility of
  // the message center.
  bool ShouldShowMessageCenter() const;

  NotificationBubbleWrapper* message_center_bubble() const {
    return message_center_bubble_.get();
  }

  // Testing accessors.
  bool IsPopupVisible() const;
  MessageCenterBubble* GetMessageCenterBubbleForTest();

  aura::Window* status_area_window_;
  std::unique_ptr<MessageCenterUiController> message_center_ui_controller_;
  std::unique_ptr<NotificationBubbleWrapper> message_center_bubble_;
  std::unique_ptr<message_center::MessagePopupCollection> popup_collection_;
  std::unique_ptr<views::View> bell_icon_;
  std::unique_ptr<views::View> quiet_mode_icon_;
  std::unique_ptr<NotificationTrayLabelSubview> counter_;

  scoped_refptr<gfx::AnimationContainer> animation_container_ =
      new gfx::AnimationContainer();

  std::unordered_map<std::string, std::unique_ptr<NotificationTrayImageSubview>>
      visible_small_icons_;

  bool show_message_center_on_unlock_;

  bool should_update_tray_content_;

  std::unique_ptr<AshPopupAlignmentDelegate> popup_alignment_delegate_;

  DISALLOW_COPY_AND_ASSIGN(NotificationTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_NOTIFICATION_TRAY_H_
