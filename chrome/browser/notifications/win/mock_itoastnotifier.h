// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_WIN_MOCK_ITOASTNOTIFIER_H_
#define CHROME_BROWSER_NOTIFICATIONS_WIN_MOCK_ITOASTNOTIFIER_H_

#include <windows.ui.notifications.h>
#include <wrl/implements.h>

#include "base/callback.h"
#include "base/macros.h"

class NotificationLaunchId;

class MockIToastNotifier
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::UI::Notifications::IToastNotifier> {
 public:
  MockIToastNotifier();
  ~MockIToastNotifier() override;

  // Sets a callback to be notified when Show has been called.
  void SetNotificationShownCallback(
      const base::RepeatingCallback<
          void(const NotificationLaunchId& launch_id)>& callback);

  // ABI::Windows::UI::Notifications::IToastNotifier implementation:
  HRESULT STDMETHODCALLTYPE
  Show(ABI::Windows::UI::Notifications::IToastNotification* notification)
      override;

  HRESULT STDMETHODCALLTYPE
  Hide(ABI::Windows::UI::Notifications::IToastNotification* notification)
      override;

  HRESULT STDMETHODCALLTYPE get_Setting(
      ABI::Windows::UI::Notifications::NotificationSetting* value) override;

  HRESULT STDMETHODCALLTYPE AddToSchedule(
      ABI::Windows::UI::Notifications::IScheduledToastNotification*
          scheduledToast) override;

  HRESULT STDMETHODCALLTYPE RemoveFromSchedule(
      ABI::Windows::UI::Notifications::IScheduledToastNotification*
          scheduledToast) override;

  HRESULT STDMETHODCALLTYPE GetScheduledToastNotifications(
      __FIVectorView_1_Windows__CUI__CNotifications__CScheduledToastNotification**
          scheduledToasts) override;

 private:
  base::RepeatingCallback<void(const NotificationLaunchId& launch_id)>
      notification_shown_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockIToastNotifier);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_WIN_MOCK_ITOASTNOTIFIER_H_
