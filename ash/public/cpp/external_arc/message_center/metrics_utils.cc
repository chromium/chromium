// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/message_center/metrics_utils.h"

#include "base/metrics/histogram_functions.h"

namespace ash::metrics_utils {

void LogArcNotificationActionEnabled(bool action_enabled) {
  base::UmaHistogramBoolean("Arc.Notifications.ActionEnabled", action_enabled);
}

void LogArcNotificationInlineReplyEnabled(bool inline_reply_enabled) {
  base::UmaHistogramBoolean("Arc.Notifications.InlineReplyEnabled",
                            inline_reply_enabled);
}

void LogArcNotificationStyle(arc::mojom::ArcNotificationStyle style) {
  base::UmaHistogramEnumeration("Arc.Notifications.Style", style);
}

void LogArcNotificationIsCustomNotification(bool is_custom_notification) {
  base::UmaHistogramBoolean("Arc.Notifications.IsCustomNotification",
                            is_custom_notification);
}

}  // namespace ash::metrics_utils
