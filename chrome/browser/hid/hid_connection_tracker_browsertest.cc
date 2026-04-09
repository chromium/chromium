// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_notifications/device_connection_tracker_test_base.h"
#include "chrome/browser/hid/hid_connection_tracker_factory.h"
#include "chrome/browser/hid/hid_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class HidConnectionTrackerTest : public DeviceConnectionTrackerTestBase {
 public:
  HidConnectionTrackerTest() = default;
  HidConnectionTrackerTest(const HidConnectionTrackerTest&) = delete;
  HidConnectionTrackerTest& operator=(const HidConnectionTrackerTest&) = delete;
  ~HidConnectionTrackerTest() override = default;

  void SetUpOnMainThread() override {
    DeviceConnectionTrackerTestBase::SetUpOnMainThread();
    auto hid_system_tray_icon = std::make_unique<TestHidSystemTrayIcon>();
    g_browser_process->set_hid_system_tray_icon_for_test(
        std::move(hid_system_tray_icon));
  }

  void TearDownOnMainThread() override {
    // Set the system tray icon to null to avoid uninteresting call to it during
    // profile destruction.
    g_browser_process->set_hid_system_tray_icon_for_test(nullptr);
    DeviceConnectionTrackerTestBase::TearDownOnMainThread();
  }

  DeviceConnectionTracker* GetDeviceConnectionTracker(Profile* profile,
                                                      bool create) override {
    return HidConnectionTrackerFactory::GetForProfile(profile, create);
  }

  MockDeviceSystemTrayIcon* GetMockDeviceSystemTrayIcon() override {
    TestHidSystemTrayIcon* test_hid_system_tray_icon =
        static_cast<TestHidSystemTrayIcon*>(
            g_browser_process->hid_system_tray_icon());

    if (!test_hid_system_tray_icon) {
      return nullptr;
    }

    return test_hid_system_tray_icon->mock_device_system_tray_icon();
  }
};

#if BUILDFLAG(ENABLE_EXTENSIONS)
IN_PROC_BROWSER_TEST_F(HidConnectionTrackerTest,
                       DeviceConnectionExtensionOrigins) {
  TestDeviceConnectionExtensionOrigins(/*has_system_tray_icon=*/true);
}

IN_PROC_BROWSER_TEST_F(HidConnectionTrackerTest,
                       DeviceConnectionExtensionOriginsWithNullSystemTrayIcon) {
  g_browser_process->set_hid_system_tray_icon_for_test(nullptr);
  TestDeviceConnectionExtensionOrigins(/*has_system_tray_icon=*/false);
}

IN_PROC_BROWSER_TEST_F(HidConnectionTrackerTest,
                       ProfileDestroyedExtensionOrigin) {
  TestProfileDestroyedExtensionOrigin();
}

IN_PROC_BROWSER_TEST_F(HidConnectionTrackerTest, WhitelistedGnubbyDev) {
  TestSingleProfileWhitelistedExtension("gnubbyd-v3 dev",
                                        "ckcendljdlmgnhghiaomidhiiclmapok");
}

IN_PROC_BROWSER_TEST_F(HidConnectionTrackerTest, WhitelistedGnubbyProd) {
  TestSingleProfileWhitelistedExtension("gnubbyd-v3 prod",
                                        "lfboplenmmjcmpbkeemecobbadnmpfhi");
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
