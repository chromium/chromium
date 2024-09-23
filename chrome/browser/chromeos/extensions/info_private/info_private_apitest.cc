// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/stylus_utils.h"
#include "base/command_line.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/values.h"
#include "build/config/chromebox_for_meetings/buildflags.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/events/test/event_generator.h"

using base::test::ScopedChromeOSVersionInfo;

namespace {

const char kTestAppId[] = "ljoammodoonkhnehlncldjelhidljdpi";

}  // namespace

class ChromeOSInfoPrivateTest : public extensions::ExtensionApiTest {
 public:
  ChromeOSInfoPrivateTest() {}
  ~ChromeOSInfoPrivateTest() override {}

 protected:
  void EnableKioskSession() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kForceAppMode);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(switches::kAppId,
                                                              kTestAppId);
  }
};

// Flaky crashes. https://crbug.com/1226266
IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, DISABLED_TestGetAndSet) {
  // Set the initial timezone different from what JS function
  // timezoneSetTest() will attempt to set.
  profile()->GetPrefs()->SetString(prefs::kUserTimezone, "America/Los_Angeles");

  // Check that accessibility settings are set to default values.
  PrefService* prefs = profile()->GetPrefs();
  ASSERT_FALSE(prefs->GetBoolean(ash::prefs::kAccessibilityLargeCursorEnabled));
  ASSERT_FALSE(prefs->GetBoolean(ash::prefs::kAccessibilityStickyKeysEnabled));
  ASSERT_FALSE(
      prefs->GetBoolean(ash::prefs::kAccessibilitySpokenFeedbackEnabled));
  ASSERT_FALSE(
      prefs->GetBoolean(ash::prefs::kAccessibilityHighContrastEnabled));
  ASSERT_FALSE(prefs->GetBoolean(ash::prefs::kAccessibilityAutoclickEnabled));
  ASSERT_FALSE(prefs->GetBoolean(ash::prefs::kAccessibilityCursorColorEnabled));

  ASSERT_FALSE(
      profile()->GetPrefs()->GetBoolean(ash::prefs::kSendFunctionKeys));

  ASSERT_TRUE(RunExtensionTest("chromeos_info_private/basic", {},
                               {.load_as_component = true}))
      << message_;

  // Check that all accessibility settings have been flipped by the test.
  ASSERT_TRUE(prefs->GetBoolean(ash::prefs::kAccessibilityLargeCursorEnabled));
  ASSERT_TRUE(prefs->GetBoolean(ash::prefs::kAccessibilityStickyKeysEnabled));
  ASSERT_TRUE(
      prefs->GetBoolean(ash::prefs::kAccessibilitySpokenFeedbackEnabled));
  ASSERT_TRUE(prefs->GetBoolean(ash::prefs::kAccessibilityHighContrastEnabled));
  ASSERT_TRUE(prefs->GetBoolean(ash::prefs::kAccessibilityAutoclickEnabled));
  ASSERT_TRUE(prefs->GetBoolean(ash::prefs::kAccessibilityCursorColorEnabled));

  ASSERT_TRUE(prefs->GetBoolean(ash::prefs::kSendFunctionKeys));
}

// docked magnifier and screen magnifier are mutually exclusive. test each of
// them one by one.

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, TestGetAndSetDockedMagnifier) {
  PrefService* prefs = profile()->GetPrefs();
  ASSERT_FALSE(prefs->GetBoolean(ash::prefs::kDockedMagnifierEnabled));

  ASSERT_TRUE(RunExtensionTest("chromeos_info_private/basic",
                               {.custom_arg = "dockedMagnifier"},
                               {.load_as_component = true}))
      << message_;

  ASSERT_TRUE(prefs->GetBoolean(ash::prefs::kDockedMagnifierEnabled));
}

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, TestGetAndSetScreenMagnifier) {
  PrefService* prefs = profile()->GetPrefs();
  ASSERT_FALSE(
      prefs->GetBoolean(ash::prefs::kAccessibilityScreenMagnifierEnabled));

  ASSERT_TRUE(RunExtensionTest("chromeos_info_private/basic",
                               {.custom_arg = "screenMagnifier"},
                               {.load_as_component = true}))
      << message_;

  ASSERT_TRUE(
      prefs->GetBoolean(ash::prefs::kAccessibilityScreenMagnifierEnabled));
}

// TODO(steel): Investigate merging the following tests.

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, Kiosk) {
  EnableKioskSession();
  ASSERT_TRUE(
      RunExtensionTest("chromeos_info_private/extended",
                       {.custom_arg = "kiosk", .launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, ArcNotAvailable) {
  ASSERT_TRUE(RunExtensionTest(
      "chromeos_info_private/extended",
      {.custom_arg = "arc not-available", .launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, Chromebase) {
  ScopedChromeOSVersionInfo version("DEVICETYPE=CHROMEBASE", base::Time::Now());
  ASSERT_TRUE(RunExtensionTest(
      "chromeos_info_private/extended",
      {.custom_arg = "chromebase", .launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, Chromebit) {
  ScopedChromeOSVersionInfo version("DEVICETYPE=CHROMEBIT", base::Time::Now());
  ASSERT_TRUE(RunExtensionTest(
      "chromeos_info_private/extended",
      {.custom_arg = "chromebit", .launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, Chromebook) {
  ScopedChromeOSVersionInfo version("DEVICETYPE=CHROMEBOOK", base::Time::Now());
  ASSERT_TRUE(RunExtensionTest(
      "chromeos_info_private/extended",
      {.custom_arg = "chromebook", .launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, Chromebox) {
  ScopedChromeOSVersionInfo version("DEVICETYPE=CHROMEBOX", base::Time::Now());
  ASSERT_TRUE(RunExtensionTest(
      "chromeos_info_private/extended",
      {.custom_arg = "chromebox", .launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, UnknownDeviceType) {
  ScopedChromeOSVersionInfo version("DEVICETYPE=UNKNOWN", base::Time::Now());
  ASSERT_TRUE(RunExtensionTest(
      "chromeos_info_private/extended",
      {.custom_arg = "unknown device type", .launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, AssistantSupported) {
  ASSERT_TRUE(RunExtensionTest(
      "chromeos_info_private/extended",
      {.custom_arg = "assistant supported", .launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, StylusUnsupported) {
  ASSERT_TRUE(RunExtensionTest(
      "chromeos_info_private/extended",
      {.custom_arg = "stylus unsupported", .launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, StylusSupported) {
  ui::DeviceDataManagerTestApi test_api;
  ui::TouchscreenDevice touchscreen(1,
                                    ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                                    "Touchscreen", gfx::Size(1024, 768), 0);
  touchscreen.has_stylus = true;
  test_api.SetTouchscreenDevices({touchscreen});

  ASSERT_TRUE(RunExtensionTest(
      "chromeos_info_private/extended",
      {.custom_arg = "stylus supported", .launch_as_platform_app = true}))
      << message_;
}

// TODO(crbug.com/40564126): Excluded from Mash because pointer events
// aren't seen.
IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, StylusSeen) {
  ui::DeviceDataManagerTestApi test_api;
  ui::TouchscreenDevice touchscreen(1,
                                    ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                                    "Touchscreen", gfx::Size(1024, 768), 0);
  touchscreen.has_stylus = true;
  test_api.SetTouchscreenDevices({touchscreen});

  ui::test::EventGenerator generator(
      browser()->window()->GetNativeWindow()->GetRootWindow());
  generator.EnterPenPointerMode();
  generator.PressTouch();
  generator.ReleaseTouch();
  generator.ExitPenPointerMode();

  ASSERT_TRUE(RunExtensionTest(
      "chromeos_info_private/extended",
      {.custom_arg = "stylus seen", .launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, TestGetIsMeetDevice) {
  const char* custom_arg =
#if BUILDFLAG(PLATFORM_CFM)
      "Is Meet Device - True";
#else
      "Is Meet Device - False";
#endif

  ASSERT_TRUE(RunExtensionTest(
      "chromeos_info_private/extended",
      {.custom_arg = custom_arg, .launch_as_platform_app = true}))
      << message_;
}

class ChromeOSInfoPrivateInternalStylusTest : public ChromeOSInfoPrivateTest {
 public:
  ChromeOSInfoPrivateInternalStylusTest() = default;

  ChromeOSInfoPrivateInternalStylusTest(
      const ChromeOSInfoPrivateInternalStylusTest&) = delete;
  ChromeOSInfoPrivateInternalStylusTest& operator=(
      const ChromeOSInfoPrivateInternalStylusTest&) = delete;

  ~ChromeOSInfoPrivateInternalStylusTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeOSInfoPrivateTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kHasInternalStylus);
  }
};

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateInternalStylusTest,
                       StylusSeenInternal) {
  ash::stylus_utils::SetHasStylusInputForTesting();
  ASSERT_TRUE(RunExtensionTest(
      "chromeos_info_private/extended",
      {.custom_arg = "stylus seen", .launch_as_platform_app = true}))
      << message_;
}

class ChromeOSArcInfoPrivateTest : public ChromeOSInfoPrivateTest {
 public:
  ChromeOSArcInfoPrivateTest() = default;

  ChromeOSArcInfoPrivateTest(const ChromeOSArcInfoPrivateTest&) = delete;
  ChromeOSArcInfoPrivateTest& operator=(const ChromeOSArcInfoPrivateTest&) =
      delete;

  ~ChromeOSArcInfoPrivateTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    // Make ARC enabled for ArcAvailable/ArcEnabled tests.
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(ChromeOSArcInfoPrivateTest, ArcEnabled) {
  ASSERT_TRUE(RunExtensionTest(
      "chromeos_info_private/extended",
      {.custom_arg = "arc enabled", .launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ChromeOSArcInfoPrivateTest, ArcAvailable) {
  // Even if ARC is available, ARC may not be able to be enabled. (Please
  // see arc::IsArcAllowedForProfile() for details).
  // In such cases, we expect "available". However, current testing framework
  // does not seem to run with such cases, unfortunately. So, here directly
  // control the function.
  arc::DisallowArcForTesting();
  ASSERT_TRUE(RunExtensionTest(
      "chromeos_info_private/extended",
      {.custom_arg = "arc available", .launch_as_platform_app = true}))
      << message_;
}

class ChromeOSManagedDeviceInfoPrivateTest : public ChromeOSInfoPrivateTest {
 public:
  ChromeOSManagedDeviceInfoPrivateTest() = default;

  ChromeOSManagedDeviceInfoPrivateTest(
      const ChromeOSManagedDeviceInfoPrivateTest&) = delete;
  ChromeOSManagedDeviceInfoPrivateTest& operator=(
      const ChromeOSManagedDeviceInfoPrivateTest&) = delete;

  ~ChromeOSManagedDeviceInfoPrivateTest() override = default;

 private:
  ash::ScopedStubInstallAttributes test_install_attributes_{
      ash::StubInstallAttributes::CreateCloudManaged("fake-domain", "fake-id")};
};

IN_PROC_BROWSER_TEST_F(ChromeOSManagedDeviceInfoPrivateTest, Managed) {
  ASSERT_TRUE(RunExtensionTest(
      "chromeos_info_private/extended",
      {.custom_arg = "managed", .launch_as_platform_app = true}))
      << message_;
}

class ChromeOSInfoPrivateDeviceRequisitionTest
    : public ChromeOSInfoPrivateTest {
 public:
  ChromeOSInfoPrivateDeviceRequisitionTest() {
    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kValid);
  }

  ChromeOSInfoPrivateDeviceRequisitionTest(
      const ChromeOSInfoPrivateDeviceRequisitionTest&) = delete;
  ChromeOSInfoPrivateDeviceRequisitionTest& operator=(
      const ChromeOSInfoPrivateDeviceRequisitionTest&) = delete;

  ~ChromeOSInfoPrivateDeviceRequisitionTest() override = default;

 protected:
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateDeviceRequisitionTest,
                       TestDeviceRequisitionUnset) {
  fake_statistics_provider_.ClearMachineStatistic(
      ash::system::kOemDeviceRequisitionKey);
  ASSERT_TRUE(RunExtensionTest("chromeos_info_private/extended",
                               {.custom_arg = "Device Requisition - Unset",
                                .launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateDeviceRequisitionTest,
                       TestDeviceRequisitionRemora) {
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kOemDeviceRequisitionKey, "remora");
  ASSERT_TRUE(RunExtensionTest("chromeos_info_private/extended",
                               {.custom_arg = "Device Requisition - Remora",
                                .launch_as_platform_app = true}))
      << message_;
}
