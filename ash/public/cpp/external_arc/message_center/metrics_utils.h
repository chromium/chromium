// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_METRICS_UTILS_H_
#define ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_METRICS_UTILS_H_

#include "ash/components/arc/mojom/notifications.mojom.h"

namespace ash::metrics_utils {

// Note to keep in sync with enum in tools/metrics/histograms/enums.xml.
enum class ArcNotificationExpandState {
  // No expand button is available, the size of the notification is fixed
  kFixedSize = 0,
  // Expand button is available to expand the notification
  kExpandable = 1,
  // The state after the user expands the notification
  kExpanded = 2,
  // The state after the user collapses the notification
  kCollapsed = 3,
  kMaxValue = kCollapsed,
};

// Logs if action button is enabled for Arc notification.
void LogArcNotificationActionEnabled(bool action_enabled);

// Logs the expand state for Arc notification.
void LogArcNotificationExpandState(ArcNotificationExpandState state);

// Logs if inline reply is enabled for Arc notification.
void LogArcNotificationInlineReplyEnabled(bool inline_reply_enabled);

// Logs the style of Arc rich notification.
void LogArcNotificationStyle(arc::mojom::ArcNotificationStyle style);

// Logs if the notification is custom notification.
void LogArcNotificationIsCustomNotification(bool is_custom_notification);

}  // namespace ash::metrics_utils

#endif  // ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_METRICS_UTILS_H_
