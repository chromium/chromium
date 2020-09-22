// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_UTILS_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_UTILS_H_

#include "base/strings/string16.h"
#include "ui/message_center/public/cpp/notification.h"

// This file is a combination of methods that are shared between the macOS
// notification bridges.

base::string16 CreateMacNotificationTitle(
    const message_center::Notification& notification);

base::string16 CreateMacNotificationContext(
    bool is_persistent,
    const message_center::Notification& notification,
    bool requires_attribution);

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_UTILS_H_
