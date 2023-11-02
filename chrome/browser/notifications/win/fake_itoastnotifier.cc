// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/win/fake_itoastnotifier.h"

#include "chrome/browser/notifications/win/notification_launch_id.h"
#include "chrome/browser/notifications/win/notification_util.h"

namespace winui = ABI::Windows::UI;

FakeIToastNotifier::FakeIToastNotifier() = default;
FakeIToastNotifier::~FakeIToastNotifier() = default;

void FakeIToastNotifier::SetNotificationShownCallback(
    const base::RepeatingCallback<void(const NotificationLaunchId& launch_id)>&
        callback) {
  notification_shown_callback_ = callback;
}

HRESULT FakeIToastNotifier::Show(
    winui::Notifications::IToastNotification* notification) {
  if (!notification_shown_callback_)
    return S_OK;

  NotificationLaunchId launch_id = GetNotificationLaunchId(notification);
  notification_shown_callback_.Run(launch_id);
  return S_OK;
}

HRESULT FakeIToastNotifier::Hide(
    winui::Notifications::IToastNotification* notification) {
  return E_NOTIMPL;
}

HRESULT FakeIToastNotifier::get_Setting(
    winui::Notifications::NotificationSetting* value) {
  *value = winui::Notifications::NotificationSetting_Enabled;
  return S_OK;
}

HRESULT FakeIToastNotifier::AddToSchedule(
    winui::Notifications::IScheduledToastNotification* scheduledToast) {
  return E_NOTIMPL;
}

HRESULT FakeIToastNotifier::RemoveFromSchedule(
    winui::Notifications::IScheduledToastNotification* scheduledToast) {
  return E_NOTIMPL;
}

HRESULT FakeIToastNotifier::GetScheduledToastNotifications(
    __FIVectorView_1_Windows__CUI__CNotifications__CScheduledToastNotification**
        scheduledToasts) {
  return E_NOTIMPL;
}
