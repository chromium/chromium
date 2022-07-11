// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_METRICS_UTILS_H_
#define ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_METRICS_UTILS_H_

#include "ash/components/arc/mojom/notifications.mojom.h"

namespace ash::metrics_utils {

// Logs if action button is enabled for Arc notification.
void LogArcNotificationActionEnabled(bool action_enabled);

// Logs the style of Arc rich notification.
void LogArcNotificationStyle(arc::mojom::ArcNotificationStyle style);

}  // namespace ash::metrics_utils

#endif  // ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_METRICS_UTILS_H_
