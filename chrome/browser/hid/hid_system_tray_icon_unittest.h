// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_SYSTEM_TRAY_ICON_UNITTEST_H_
#define CHROME_BROWSER_HID_HID_SYSTEM_TRAY_ICON_UNITTEST_H_

#include <string>

#include "chrome/browser/hid/hid_connection_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockHidConnectionTracker : public HidConnectionTracker {
 public:
  explicit MockHidConnectionTracker(Profile* profile);
  ~MockHidConnectionTracker() override;
  MOCK_METHOD(void, ShowHidContentSettingsExceptions, (), (override));
};

class HidSystemTrayIconTestBase : public BrowserWithTestWindowTest {
 public:
  // Check if the hid system tray icon is shown and all the action buttons work
  // correctly with the given |profile_connection_counts|.
  virtual void CheckIcon(const std::vector<std::pair<Profile*, size_t>>&
                             profile_connection_counts) = 0;

  // Check no hid system tray is being shown.
  virtual void CheckIconHidden() = 0;

  std::u16string GetExpectedButtonTitleForProfile(Profile* profile);
  std::u16string GetExpectedIconTooltip(size_t num_devices);

  // This is used to inject MockHidConnectionTracker.
  BrowserContextKeyedServiceFactory::TestingFactory
  GetHidConnectionTrackerTestingFactory();

  // Create a testing profile with MockHidConnectionTracker.
  Profile* CreateTestingProfile(const std::string& profile_name);

  // Test the scenario involving multiple profiles including profile
  // destruction.
  void TestMultipleProfiles();

  // Test the scenario with single profile.
  void TestSingleProfile();
};

#endif  // CHROME_BROWSER_HID_HID_SYSTEM_TRAY_ICON_UNITTEST_H_
