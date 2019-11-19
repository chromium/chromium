// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/stylus_utils.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/arc/arc_util.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/events/test/event_generator.h"

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

  void SetDeviceType(const std::string& device_type) {
    const std::string lsb_release = std::string("DEVICETYPE=") + device_type;
    base::SysInfo::SetChromeOSVersionInfoForTest(lsb_release,
                                                 base::Time::Now());
  }
};

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, TestGetAndSet) {
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
  ASSERT_FALSE(
      prefs->GetBoolean(ash::prefs::kAccessibilityScreenMagnifierEnabled));
  ASSERT_FALSE(prefs->GetBoolean(ash::prefs::kAccessibilityAutoclickEnabled));

  ASSERT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kLanguageSendFunctionKeys));
  ASSERT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kCameraMediaConsolidated));

  ASSERT_TRUE(RunComponentExtensionTest("chromeos_info_private/basic"))
      << message_;

  // Check that all accessibility settings have been flipped by the test.
  ASSERT_TRUE(prefs->GetBoolean(ash::prefs::kAccessibilityLargeCursorEnabled));
  ASSERT_TRUE(prefs->GetBoolean(ash::prefs::kAccessibilityStickyKeysEnabled));
  ASSERT_TRUE(
      prefs->GetBoolean(ash::prefs::kAccessibilitySpokenFeedbackEnabled));
  ASSERT_TRUE(prefs->GetBoolean(ash::prefs::kAccessibilityHighContrastEnabled));
  ASSERT_TRUE(
      prefs->GetBoolean(ash::prefs::kAccessibilityScreenMagnifierEnabled));
  ASSERT_TRUE(prefs->GetBoolean(ash::prefs::kAccessibilityAutoclickEnabled));

  ASSERT_TRUE(prefs->GetBoolean(prefs::kLanguageSendFunctionKeys));
  ASSERT_TRUE(prefs->GetBoolean(prefs::kCameraMediaConsolidated));
}

// TODO(steel): Investigate merging the following tests.

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, Kiosk) {
  EnableKioskSession();
  ASSERT_TRUE(
      RunPlatformAppTestWithArg("chromeos_info_private/extended", "kiosk"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, ArcNotAvailable) {
  ASSERT_TRUE(RunPlatformAppTestWithArg("chromeos_info_private/extended",
                                        "arc not-available"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, Chromebase) {
  SetDeviceType("CHROMEBASE");
  ASSERT_TRUE(
      RunPlatformAppTestWithArg("chromeos_info_private/extended", "chromebase"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, Chromebit) {
  SetDeviceType("CHROMEBIT");
  ASSERT_TRUE(
      RunPlatformAppTestWithArg("chromeos_info_private/extended", "chromebit"))
      << message_;
}
IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, Chromebook) {
  SetDeviceType("CHROMEBOOK");
  ASSERT_TRUE(
      RunPlatformAppTestWithArg("chromeos_info_private/extended", "chromebook"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, Chromebox) {
  SetDeviceType("CHROMEBOX");
  ASSERT_TRUE(
      RunPlatformAppTestWithArg("chromeos_info_private/extended", "chromebox"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, UnknownDeviceType) {
  SetDeviceType("UNKNOWN");
  ASSERT_TRUE(RunPlatformAppTestWithArg("chromeos_info_private/extended",
                                        "unknown device type"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, AssistantSupported) {
  ASSERT_TRUE(RunPlatformAppTestWithArg("chromeos_info_private/extended",
                                        "assistant supported"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, StylusUnsupported) {
  ASSERT_TRUE(RunPlatformAppTestWithArg("chromeos_info_private/extended",
                                        "stylus unsupported"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateTest, StylusSupported) {
  ui::DeviceDataManagerTestApi test_api;
  ui::TouchscreenDevice touchscreen(1,
                                    ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                                    "Touchscreen", gfx::Size(1024, 768), 0);
  touchscreen.has_stylus = true;
  test_api.SetTouchscreenDevices({touchscreen});

  ASSERT_TRUE(RunPlatformAppTestWithArg("chromeos_info_private/extended",
                                        "stylus supported"))
      << message_;
}

// TODO(https://crbug.com/814675): Excluded from Mash because pointer events
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

  ASSERT_TRUE(RunPlatformAppTestWithArg("chromeos_info_private/extended",
                                        "stylus seen"))
      << message_;
}

class ChromeOSInfoPrivateInternalStylusTest : public ChromeOSInfoPrivateTest {
 public:
  ChromeOSInfoPrivateInternalStylusTest() = default;
  ~ChromeOSInfoPrivateInternalStylusTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeOSInfoPrivateTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kHasInternalStylus);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeOSInfoPrivateInternalStylusTest);
};

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateInternalStylusTest,
                       StylusSeenInternal) {
  ash::stylus_utils::SetHasStylusInputForTesting();
  ASSERT_TRUE(RunPlatformAppTestWithArg("chromeos_info_private/extended",
                                        "stylus seen"))
      << message_;
}

class ChromeOSArcInfoPrivateTest : public ChromeOSInfoPrivateTest {
 public:
  ChromeOSArcInfoPrivateTest() = default;
  ~ChromeOSArcInfoPrivateTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    // Make ARC enabled for ArcAvailable/ArcEnabled tests.
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeOSArcInfoPrivateTest);
};

IN_PROC_BROWSER_TEST_F(ChromeOSArcInfoPrivateTest, ArcEnabled) {
  ASSERT_TRUE(RunPlatformAppTestWithArg("chromeos_info_private/extended",
                                        "arc enabled"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ChromeOSArcInfoPrivateTest, ArcAvailable) {
  // Even if ARC is available, ARC may not be able to be enabled. (Please
  // see arc::IsArcAllowedForProfile() for details).
  // In such cases, we expect "available". However, current testing framework
  // does not seem to run with such cases, unfortunately. So, here directly
  // control the function.
  arc::DisallowArcForTesting();
  ASSERT_TRUE(RunPlatformAppTestWithArg("chromeos_info_private/extended",
                                        "arc available"))
      << message_;
}

class ChromeOSManagedDeviceInfoPrivateTest : public ChromeOSInfoPrivateTest {
 public:
  ChromeOSManagedDeviceInfoPrivateTest() = default;
  ~ChromeOSManagedDeviceInfoPrivateTest() override = default;

 private:
  chromeos::ScopedStubInstallAttributes test_install_attributes_{
      chromeos::StubInstallAttributes::CreateCloudManaged("fake-domain",
                                                          "fake-id")};

  DISALLOW_COPY_AND_ASSIGN(ChromeOSManagedDeviceInfoPrivateTest);
};

IN_PROC_BROWSER_TEST_F(ChromeOSManagedDeviceInfoPrivateTest, Managed) {
  ASSERT_TRUE(
      RunPlatformAppTestWithArg("chromeos_info_private/extended", "managed"))
      << message_;
}
