// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_SYSTEM_TRAY_ICON_UNITTEST_H_
#define CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_SYSTEM_TRAY_ICON_UNITTEST_H_

#include <string>
#include <tuple>

#include "chrome/browser/device_notifications/device_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class DeviceSystemTrayIconTestBase : public BrowserWithTestWindowTest {
 public:
  using OriginItem = std::tuple<url::Origin, int, std::string>;
  using ProfileItem = std::pair<Profile*, std::vector<OriginItem>>;

  DeviceSystemTrayIconTestBase()
      : BrowserWithTestWindowTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void TearDown() override;

  // Check if the device system tray icon is shown and all the action buttons
  // work correctly with the given |profile_connection_counts|.
  virtual void CheckIcon(
      const std::vector<ProfileItem>& profile_connection_counts) = 0;

  // Check no device system tray is being shown.
  virtual void CheckIconHidden() = 0;

  // Reset the device system tray icon of the testing browser process.
  virtual void ResetTestingBrowserProcessSystemTrayIcon() = 0;

  std::u16string GetExpectedButtonTitleForProfile(Profile* profile);

  // Get the expected title for the device system tray icon.
  virtual std::u16string GetExpectedTitle(size_t num_origins,
                                          size_t num_connections) = 0;

  // This is used to inject MockDeviceConnectionTracker to the device type's
  // connection tracker of the `profile`.
  virtual void SetDeviceConnectionTrackerTestingFactory(Profile* profile) = 0;

  virtual DeviceConnectionTracker* GetDeviceConnectionTracker(Profile* profile,
                                                              bool create) = 0;

  // Get the mock device connection tracker from the `connection_tracker`.
  virtual MockDeviceConnectionTracker* GetMockDeviceConnectionTracker(
      DeviceConnectionTracker* connection_tracker) = 0;

  // Create a testing profile with MockDeviceConnectionTracker.
  Profile* CreateTestingProfile(const std::string& profile_name);

  // Test the scenario involving multiple profiles including profile
  // destruction.
  void TestMultipleProfiles(
      const std::vector<
          std::pair<Profile*,
                    std::vector<std::pair<url::Origin, std::string>>>>&
          profile_origins_pairs);

  // Test the scenario when a device connection is bouncing.
  void TestBounceConnection(Profile* profile, const url::Origin& origin);

  // Test the scenario with single profile.
  void TestSingleProfile(Profile* profile,
                         const url::Origin& origin1,
                         const url::Origin& origin2);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Create a extension with |extension_name|.
  scoped_refptr<const extensions::Extension> CreateExtensionWithName(
      const std::string& extension_name);

  // Add the |extension| to the |profile|'s extension service.
  void AddExtensionToProfile(Profile* profile,
                             const extensions::Extension* extension);

  // Unload the |extension| from the |profile|'s extension service.
  void UnloadExtensionFromProfile(Profile* profile,
                                  const extensions::Extension* extension);

  // Run TestMultipleProfiles with extension origins.
  void TestSingleProfileExtentionOrigins();

  // Run TestBounceConnection with extension origins.
  void TestBounceConnectionExtensionOrigins();

  // Run TestMultipleProfiles with extension origins.
  void TestMultipleProfilesExtensionOrigins();

  // Test the scenario of removing an extension.
  void TestExtensionRemoval();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
};

#endif  // CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_SYSTEM_TRAY_ICON_UNITTEST_H_
