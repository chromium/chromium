// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_TEST_UTILS_H_
#define CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_TEST_UTILS_H_

#include "chrome/browser/device_notifications/device_connection_tracker.h"
#include "chrome/browser/device_notifications/device_system_tray_icon.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockDeviceSystemTrayIcon : public DeviceSystemTrayIcon {
 public:
  MockDeviceSystemTrayIcon();
  ~MockDeviceSystemTrayIcon() override;

  MOCK_METHOD(void, StageProfile, (Profile*), (override));
  MOCK_METHOD(void, UnstageProfile, (Profile*, bool), (override));
  MOCK_METHOD(void, ProfileAdded, (Profile*), (override));
  MOCK_METHOD(void, ProfileRemoved, (Profile*), (override));
  MOCK_METHOD(void, NotifyConnectionCountUpdated, (Profile*), (override));
  MOCK_METHOD(const gfx::VectorIcon&, GetIcon, (), (override));
  MOCK_METHOD(std::u16string, GetTitleLabel, (size_t, size_t), (override));
  MOCK_METHOD(std::u16string, GetContentSettingsLabel, (), (override));
  MOCK_METHOD(DeviceConnectionTracker*,
              GetConnectionTracker,
              (base::WeakPtr<Profile>),
              (override));
};

class MockDeviceConnectionTracker : public DeviceConnectionTracker {
 public:
  explicit MockDeviceConnectionTracker(Profile* profile);
  ~MockDeviceConnectionTracker() override;
  MOCK_METHOD(void, ShowContentSettingsExceptions, (), (override));
  MOCK_METHOD(void, ShowSiteSettings, (const url::Origin&), (override));
  MOCK_METHOD(DeviceSystemTrayIcon*, GetSystemTrayIcon, (), (override));
};

#endif  // CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_TEST_UTILS_H_
