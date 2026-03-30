// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/core/finds_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace finds {

void RecordNotificationInteraction(
    FindsNotificationUserInteraction interaction) {
  base::UmaHistogramEnumeration(
      "Notifications.ChromeFinds.NotificationInteraction", interaction);
}

void RecordNotificationShown() {
  base::UmaHistogramBoolean("Notifications.ChromeFinds.NotificationShown",
                            true);
}

}  // namespace finds
