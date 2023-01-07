// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/notification_metrics.h"

#include "base/metrics/histogram_functions.h"

void LogLaunchedViaNotificationAction(NotificationActionSource source) {
  base::UmaHistogramEnumeration(
      "Notifications.macOS.LaunchedViaNotificationAction", source);
}
