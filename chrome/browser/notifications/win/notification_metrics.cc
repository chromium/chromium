// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/win/notification_metrics.h"

#include "base/metrics/histogram_macros.h"

namespace notifications_uma {

void LogDisplayHistogram(DisplayStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Notifications.Windows.DisplayStatus", status,
                            DisplayStatus::COUNT);
}

void LogCloseHistogram(CloseStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Notifications.Windows.CloseStatus", status,
                            CloseStatus::COUNT);
}

void LogHistoryHistogram(HistoryStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Notifications.Windows.HistoryStatus", status,
                            HistoryStatus::COUNT);
}

void LogGetDisplayedStatus(GetDisplayedStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Notifications.Windows.GetDisplayedStatus", status,
                            GetDisplayedStatus::COUNT);
}

void LogGetDisplayedLaunchIdStatus(GetDisplayedLaunchIdStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Notifications.Windows.GetDisplayedLaunchIdStatus",
                            status, GetDisplayedLaunchIdStatus::COUNT);
}

void LogGetNotificationLaunchIdStatus(GetNotificationLaunchIdStatus status) {
  UMA_HISTOGRAM_ENUMERATION(
      "Notifications.Windows.GetNotificationLaunchIdStatus", status,
      GetNotificationLaunchIdStatus::COUNT);
}

void LogGetSettingPolicy(GetSettingPolicy policy) {
  UMA_HISTOGRAM_ENUMERATION("Notifications.Windows.GetSettingPolicy", policy,
                            GetSettingPolicy::COUNT);
}

void LogGetSettingStatus(GetSettingStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Notifications.Windows.GetSettingStatus", status,
                            GetSettingStatus::COUNT);
}

void LogGetSettingPolicyStartup(GetSettingPolicy policy) {
  UMA_HISTOGRAM_ENUMERATION("Notifications.Windows.GetSettingPolicyStartup",
                            policy, GetSettingPolicy::COUNT);
}

void LogGetSettingStatusStartup(GetSettingStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Notifications.Windows.GetSettingStatusStartup",
                            status, GetSettingStatus::COUNT);
}

void LogHandleEventStatus(HandleEventStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Notifications.Windows.HandleEventStatus", status,
                            HandleEventStatus::COUNT);
}

void LogActivationStatus(ActivationStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Notifications.Windows.ActivationStatus", status,
                            ActivationStatus::COUNT);
}

void LogSetReadyCallbackStatus(SetReadyCallbackStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Notifications.Windows.SetReadyCallbackStatus2",
                            status, SetReadyCallbackStatus::COUNT);
}

void LogOnDismissedStatus(OnDismissedStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Notifications.Windows.OnDismissedStatus", status,
                            OnDismissedStatus::COUNT);
}

void LogOnFailedStatus(OnFailedStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Notifications.Windows.OnFailedStatus", status,
                            OnFailedStatus::COUNT);
}

}  // namespace notifications_uma
