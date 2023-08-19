// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_notifications/device_test_utils.h"
#include "chrome/browser/device_notifications/device_status_icon_renderer.h"

MockDeviceSystemTrayIcon::MockDeviceSystemTrayIcon()
    : DeviceSystemTrayIcon(nullptr) {}

MockDeviceSystemTrayIcon::~MockDeviceSystemTrayIcon() = default;

MockDeviceConnectionTracker::MockDeviceConnectionTracker(Profile* profile)
    : DeviceConnectionTracker(profile) {}

MockDeviceConnectionTracker::~MockDeviceConnectionTracker() = default;
