// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_ARC_NOTIFICATION_ITEM_H_
#define ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_ARC_NOTIFICATION_ITEM_H_

#include "ash/components/arc/mojom/notifications.mojom.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

class ArcNotificationItem {
 public:
  class Observer {
   public:
    // Invoked when the notification data for this item has changed.
    virtual void OnItemDestroying() = 0;

    // Invoked when the type of the shown content is changed.
    virtual void OnItemContentChanged(
        arc::mojom::ArcNotificationShownContents content) {}

    // Invoked when the remote input textbox on notification is activated or
    // deactivated.
    virtual void OnRemoteInputActivationChanged(bool activated) {}

   protected:
    virtual ~Observer() = default;
  };

  virtual ~ArcNotificationItem() = default;

  // Called when the notification is closed on Android-side. This is called from
  // ArcNotificationManager.
  virtual void OnClosedFromAndroid() = 0;

  // Called when the notification is updated on Android-side. This is called
  // from ArcNotificationManager.
  virtual void OnUpdatedFromAndroid(arc::mojom::ArcNotificationDataPtr data,
                                    const std::string& app_id) = 0;

  // Called when the notification is closed on Chrome-side. This is called from
  // ArcNotificationDelegate.
  virtual void Close(bool by_user) = 0;

  // Called when the notification is clicked by user. This is called from
  // ArcNotificationDelegate.
  virtual void Click() = 0;
  // Called when the notification button is clicked by user. This is called
  // from ArcNotificationDelegate.
  virtual void ClickButton(const int button_index,
                           const std::string& input) = 0;

  // Called when the user wants to open an intrinsic setting of notification.
  // This is called from ArcNotificationDelegate.
  virtual void OpenSettings() = 0;

  // Called when the user clicks 'turn off notifications' button in inline
  // settings view from the Chrome side in Chrome rendered ARC notifications.
  // Pop up the app notification settings page which allows users to disable
  // app notifications. This is called from ArcNotificationDelegate.
  virtual void DisableNotification() = 0;

  // Called when the user wants to open an intrinsic snooze setting of
  // notification. This is called from ArcNotificationDelegate.
  virtual void OpenSnooze() = 0;

  // Called when the user wants to toggle expansion of notification. This is
  // called from ArcNotificationView.
  virtual void ToggleExpansion() = 0;

  // Called when the user wants to set expand state of the notification. This
  // is called from ArcNotificationDelegate.
  virtual void SetExpandState(bool expanded) = 0;

  // Called when the notification is activated i.e. starts accepting input for
  // inline reply. Called from ArcNotificationContentView.
  virtual void OnWindowActivated(bool activated) = 0;

  // Called from ArcNotificationManager when the remote input textbox on
  // notification is activated or deactivated.
  virtual void OnRemoteInputActivationChanged(bool activate) = 0;

  // Adds an observer.
  virtual void AddObserver(Observer* observer) = 0;
  // Removes the observer.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Increments |window_ref_count_| and a CreateNotificationWindow request
  // is sent when |window_ref_count_| goes from zero to one.
  virtual void IncrementWindowRefCount() = 0;

  // Decrements |window_ref_count_| and a CloseNotificationWindow request
  // is sent when |window_ref_count_| goes from one to zero.
  virtual void DecrementWindowRefCount() = 0;

  // Returns the current snapshot.
  virtual const gfx::ImageSkia& GetSnapshot() const = 0;
  // Returns the notification type.
  virtual arc::mojom::ArcNotificationType GetNotificationType() const = 0;
  // Returns the current expand state.
  virtual arc::mojom::ArcNotificationExpandState GetExpandState() const = 0;

  virtual bool IsManuallyExpandedOrCollapsed() const = 0;

  // Cancel long press operation on Android side.
  virtual void CancelPress() = 0;

  // Returns the rect for which Android wants to handle all swipe events.
  // Defaults to the empty rectangle.
  virtual gfx::Rect GetSwipeInputRect() const = 0;
  // Returns the notification key passed from Android-side.
  virtual const std::string& GetNotificationKey() const = 0;
  // Returns the notification ID used in the Chrome message center.
  virtual const std::string& GetNotificationId() const = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_ARC_NOTIFICATION_ITEM_H_
