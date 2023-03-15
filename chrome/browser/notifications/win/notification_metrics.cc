// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/win/notification_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace notifications_uma {

void LogDisplayHistogram(DisplayStatus status) {
  base::UmaHistogramEnumeration("Notifications.Windows.DisplayStatus", status);
}

void LogCloseHistogram(CloseStatus status) {
  base::UmaHistogramEnumeration("Notifications.Windows.CloseStatus", status);
}

void LogHistoryHistogram(HistoryStatus status) {
  base::UmaHistogramEnumeration("Notifications.Windows.HistoryStatus", status);
}

void LogGetDisplayedStatus(GetDisplayedStatus status) {
  base::UmaHistogramEnumeration("Notifications.Windows.GetDisplayedStatus",
                                status);
}

void LogGetDisplayedLaunchIdStatus(GetDisplayedLaunchIdStatus status) {
  base::UmaHistogramEnumeration(
      "Notifications.Windows.GetDisplayedLaunchIdStatus", status);
}

void LogGetNotificationLaunchIdStatus(GetNotificationLaunchIdStatus status) {
  base::UmaHistogramEnumeration(
      "Notifications.Windows.GetNotificationLaunchIdStatus", status);
}

void LogGetSettingPolicy(GetSettingPolicy policy) {
  base::UmaHistogramEnumeration("Notifications.Windows.GetSettingPolicy",
                                policy);
}

void LogGetSettingStatus(GetSettingStatus status) {
  base::UmaHistogramEnumeration("Notifications.Windows.GetSettingStatus",
                                status);
}

void LogGetSettingPolicyStartup(GetSettingPolicy policy) {
  base::UmaHistogramEnumeration("Notifications.Windows.GetSettingPolicyStartup",
                                policy);
}

void LogGetSettingStatusStartup(GetSettingStatus status) {
  base::UmaHistogramEnumeration("Notifications.Windows.GetSettingStatusStartup",
                                status);
}

void LogHandleEventStatus(HandleEventStatus status) {
  base::UmaHistogramEnumeration("Notifications.Windows.HandleEventStatus",
                                status);
}

void LogActivationStatus(ActivationStatus status) {
  base::UmaHistogramEnumeration("Notifications.Windows.ActivationStatus",
                                status);
}

void LogSetReadyCallbackStatus(SetReadyCallbackStatus status) {
  base::UmaHistogramEnumeration("Notifications.Windows.SetReadyCallbackStatus2",
                                status);
}

void LogOnFailedStatus(OnFailedStatus status) {
  base::UmaHistogramEnumeration("Notifications.Windows.OnFailedStatus", status);
}

}  // namespace notifications_uma
