// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_pinned_notification.h"

#include "chrome/browser/device_notifications/device_pinned_notification_renderer.h"
#include "chrome/grit/generated_resources.h"

HidPinnedNotification::HidPinnedNotification()
    : HidSystemTrayIcon(std::make_unique<DevicePinnedNotificationRenderer>(
          this,
          "chrome://device_indicator/hid/",
          ash::NotificationCatalogName::kWebHid,
          IDS_WEBHID_SYSTEM_TRAY_ICON_EXTENSION_LIST)) {}

HidPinnedNotification::~HidPinnedNotification() = default;
