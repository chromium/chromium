// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_test_utils.h"

#include "chrome/browser/device_notifications/device_system_tray_icon_unittest.h"
#include "chrome/browser/hid/hid_connection_tracker.h"

TestHidConnectionTracker::TestHidConnectionTracker(Profile* profile)
    : HidConnectionTracker(profile), mock_device_connection_tracker_(profile) {}

TestHidConnectionTracker::~TestHidConnectionTracker() = default;

void TestHidConnectionTracker::ShowContentSettingsExceptions() {
  mock_device_connection_tracker_.ShowContentSettingsExceptions();
}
void TestHidConnectionTracker::ShowSiteSettings(const url::Origin& origin) {
  mock_device_connection_tracker_.ShowSiteSettings(origin);
}
