// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/message_center/metrics_utils.h"

#include "base/metrics/histogram_functions.h"

namespace ash::metrics_utils {

void LogArcNotificationActionEnabled(bool action_enabled) {
  base::UmaHistogramBoolean("Arc.Notifications.ActionEnabled", action_enabled);
}

void LogArcNotificationStyle(arc::mojom::ArcNotificationStyle style) {
  base::UmaHistogramEnumeration("Arc.Notifications.Style", style);
}

}  // namespace ash::metrics_utils
