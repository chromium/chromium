// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_test_utils.h"

#include "chrome/browser/device_notifications/device_status_icon_renderer.h"
#include "chrome/browser/hid/hid_connection_tracker.h"
#include "chrome/browser/hid/hid_system_tray_icon.h"

TestHidConnectionTracker::TestHidConnectionTracker(Profile* profile)
    : HidConnectionTracker(profile), mock_device_connection_tracker_(profile) {}

TestHidConnectionTracker::~TestHidConnectionTracker() = default;

void TestHidConnectionTracker::ShowContentSettingsExceptions() {
  mock_device_connection_tracker_.ShowContentSettingsExceptions();
}
void TestHidConnectionTracker::ShowSiteSettings(const url::Origin& origin) {
  mock_device_connection_tracker_.ShowSiteSettings(origin);
}

TestHidSystemTrayIcon::TestHidSystemTrayIcon() : HidSystemTrayIcon(nullptr) {}

TestHidSystemTrayIcon::~TestHidSystemTrayIcon() = default;

void TestHidSystemTrayIcon::StageProfile(Profile* profile) {
  mock_device_system_tray_icon_.StageProfile(profile);
}

void TestHidSystemTrayIcon::UnstageProfile(Profile* profile, bool immediate) {
  mock_device_system_tray_icon_.UnstageProfile(profile, immediate);
}

void TestHidSystemTrayIcon::ProfileAdded(Profile* profile) {
  mock_device_system_tray_icon_.ProfileAdded(profile);
}

void TestHidSystemTrayIcon::ProfileRemoved(Profile* profile) {
  mock_device_system_tray_icon_.ProfileRemoved(profile);
}

void TestHidSystemTrayIcon::NotifyConnectionCountUpdated(Profile* profile) {
  mock_device_system_tray_icon_.NotifyConnectionCountUpdated(profile);
}
