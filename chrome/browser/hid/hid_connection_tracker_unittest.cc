// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_notifications/device_connection_tracker_unittest.h"
#include "chrome/browser/hid/hid_connection_tracker_factory.h"
#include "chrome/browser/hid/hid_test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

class HidConnectionTrackerTest : public DeviceConnectionTrackerTestBase {
 public:
  HidConnectionTrackerTest() = default;
  HidConnectionTrackerTest(const HidConnectionTrackerTest&) = delete;
  HidConnectionTrackerTest& operator=(const HidConnectionTrackerTest&) = delete;
  ~HidConnectionTrackerTest() override = default;

  void SetUp() override {
    DeviceConnectionTrackerTestBase::SetUp();
    auto hid_system_tray_icon = std::make_unique<TestHidSystemTrayIcon>();
    TestingBrowserProcess::GetGlobal()->SetHidSystemTrayIcon(
        std::move(hid_system_tray_icon));
  }

  void TearDown() override {
    // Set the system tray icon to null to avoid uninteresting call to it during
    // profile destruction.
    TestingBrowserProcess::GetGlobal()->SetHidSystemTrayIcon(nullptr);
    DeviceConnectionTrackerTestBase::TearDown();
  }

  DeviceConnectionTracker* GetDeviceConnectionTracker(Profile* profile,
                                                      bool create) override {
    return HidConnectionTrackerFactory::GetForProfile(profile, create);
  }

  MockDeviceSystemTrayIcon* GetMockDeviceSystemTrayIcon() override {
    TestHidSystemTrayIcon* test_hid_system_tray_icon =
        static_cast<TestHidSystemTrayIcon*>(
            TestingBrowserProcess::GetGlobal()->hid_system_tray_icon());

    if (!test_hid_system_tray_icon) {
      return nullptr;
    }

    return test_hid_system_tray_icon->mock_device_system_tray_icon();
  }
};

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(HidConnectionTrackerTest, DeviceConnectionExtensionOrigins) {
  TestDeviceConnectionExtensionOrigins(/*has_system_tray_icon=*/true);
}

TEST_F(HidConnectionTrackerTest,
       DeviceConnectionExtensionOriginsWithNullSystemTrayIcon) {
  TestingBrowserProcess::GetGlobal()->SetHidSystemTrayIcon(nullptr);
  TestDeviceConnectionExtensionOrigins(/*has_system_tray_icon=*/false);
}

TEST_F(HidConnectionTrackerTest, ProfileDestroyedExtensionOrigin) {
  TestProfileDestroyedExtensionOrigin();
}

TEST_F(HidConnectionTrackerTest, WhitelistedGnubbyDev) {
  TestSingleProfileWhitelistedExtension("gnubbyd-v3 dev",
                                        "ckcendljdlmgnhghiaomidhiiclmapok");
}

TEST_F(HidConnectionTrackerTest, WhitelistedGnubbyProd) {
  TestSingleProfileWhitelistedExtension("gnubbyd-v3 prod",
                                        "lfboplenmmjcmpbkeemecobbadnmpfhi");
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
