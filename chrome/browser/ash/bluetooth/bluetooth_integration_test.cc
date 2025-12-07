// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "ash/ash_element_identifiers.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gtest_tags.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_switches.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/test/base/ash/interactive/bluetooth/bluetooth_power_state_observer.h"
#include "chrome/test/base/chromeos/crosier/annotations.h"
#include "chrome/test/base/chromeos/crosier/ash_integration_test.h"
#include "dbus/object_path.h"
#include "device/bluetooth/dbus/bluetooth_adapter_client.h"
#include "device/bluetooth/floss/floss_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace ash {
namespace {

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(BluetoothPowerStateObserver,
                                    kBluetoothPowerState);

constexpr char kBluetoothDevicesSubpagePath[] = "bluetoothDevices";

class BluetoothIntegrationTest : public AshIntegrationTest {
 public:
  BluetoothIntegrationTest() {
    // Use the legacy bluez bluetooth stack.
    feature_list_.InitAndDisableFeature(floss::features::kFlossEnabled);
  }

  // AshIntegrationTest:
  void SetUpOnMainThread() override {
    TEST_REQUIRES(crosier::Requirement::kBluetooth);
    AshIntegrationTest::SetUpOnMainThread();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(BluetoothIntegrationTest,
                       ToggleBluetoothFromQuickSettings) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-ba8a2ed6-be82-4174-bf38-489eba7181c6");
  SetupContextWidget();
  RunTestSequence(
      ObserveState(kBluetoothPowerState, BluetoothPowerStateObserver::Create()),

      // Ensure bluetooth is on at test start. If this fails it means some
      // previous test left bluetooth disabled.
      WaitForState(kBluetoothPowerState, true),

      Log("Opening quick settings bubble"), OpenQuickSettings(),

      // The bluetooth toggle button may take time to enable because the UI
      // queries the bluetooth adapter state asynchronously.
      Log("Waiting for bluetooth toggle button to enable"),
      WaitForViewProperty(kBluetoothFeatureTileToggleElementId, views::View,
                          Enabled, true),

      Log("Pressing bluetooth toggle button"),
      PressButton(kBluetoothFeatureTileToggleElementId),

      Log("Waiting for bluetooth adapter to power off"),
      WaitForState(kBluetoothPowerState, false),

      // Allow UI state to settle.

      Log("Pressing bluetooth toggle button again"),
      PressButton(kBluetoothFeatureTileToggleElementId),

      Log("Waiting for bluetooth adapter to power on"),
      WaitForState(kBluetoothPowerState, true));
}

IN_PROC_BROWSER_TEST_F(BluetoothIntegrationTest,
                       ToggleBluetoothFromQuickSettingsBluetoothPage) {
  SetupContextWidget();
  RunTestSequence(
      ObserveState(kBluetoothPowerState, BluetoothPowerStateObserver::Create()),

      // Ensure bluetooth is on at test start. If this fails it means some
      // previous test left bluetooth disabled.
      WaitForState(kBluetoothPowerState, true),

      Log("Opening quick settings bubble and navigating to the Bluetooth page"),
      OpenQuickSettings(),

      // Allow UI state to settle.

      NavigateQuickSettingsToBluetoothPage(),

      Log("Waiting for bluetooth toggle button to be shown"),
      WaitForShow(kBluetoothDetailedViewToggleElementId),

      // The bluetooth toggle button may take time to enable because the UI
      // queries the bluetooth adapter state asynchronously.
      Log("Waiting for bluetooth toggle button to enable"),
      WaitForViewProperty(kBluetoothDetailedViewToggleElementId, views::View,
                          Enabled, true),

      Log("Pressing bluetooth toggle button"),
      MoveMouseTo(kBluetoothDetailedViewToggleElementId), ClickMouse(),

      Log("Waiting for bluetooth adapter to power off"),
      WaitForState(kBluetoothPowerState, false),

      // Allow UI state to settle.

      Log("Pressing bluetooth toggle button again"),
      PressButton(kBluetoothDetailedViewToggleElementId),

      Log("Waiting for bluetooth adapter to power on"),
      WaitForState(kBluetoothPowerState, true));
}

IN_PROC_BROWSER_TEST_F(BluetoothIntegrationTest,
                       ToggleBluetoothFromOsSettings) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-d3c7f622-a376-4ca1-9be2-47a004234655");
  SetupContextWidget();

  // Ensure the OS Settings system web app (SWA) is installed.
  InstallSystemApps();

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsSettingsElementId);

  // Query to pierce through Shadow DOM to find the bluetooth toggle.
  const DeepQuery kBluetoothToggleQuery = {
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "os-settings-bluetooth-page",
      "os-settings-bluetooth-devices-subpage",
      "cr-toggle#enableBluetoothToggle",
  };

  RunTestSequence(
      // Ensure bluetooth is on at test start. If this fails it means some
      // previous test left bluetooth disabled.
      Log("Verifying initial bluetooth power state"),
      ObserveState(kBluetoothPowerState, BluetoothPowerStateObserver::Create()),
      WaitForState(kBluetoothPowerState, true),

      Log("Opening OS settings system web app"),
      InstrumentNextTab(kOsSettingsElementId, AnyBrowser()), Do([&]() {
        chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
            GetActiveUserProfile(), kBluetoothDevicesSubpagePath);
      }),
      WaitForShow(kOsSettingsElementId),

      Log("Waiting for OS settings bluetooth page to load"),
      WaitForWebContentsReady(
          kOsSettingsElementId,
          chrome::GetOSSettingsUrl(kBluetoothDevicesSubpagePath)),

      Log("Waiting for bluetooth toggle to exist"),
      WaitForElementExists(kOsSettingsElementId, kBluetoothToggleQuery),

      Log("Waiting for toggle to be checked"),
      WaitForToggleState(kOsSettingsElementId, kBluetoothToggleQuery, true),

      Log("Clicking bluetooth toggle"),
      ClickElement(kOsSettingsElementId, kBluetoothToggleQuery),

      Log("Waiting for bluetooth power off"),
      WaitForState(kBluetoothPowerState, false),

      Log("Waiting for toggle to be unchecked"),
      WaitForToggleState(kOsSettingsElementId, kBluetoothToggleQuery, false),

      Log("Clicking bluetooth toggle again"),
      ClickElement(kOsSettingsElementId, kBluetoothToggleQuery),

      Log("Waiting for bluetooth power on"),
      WaitForState(kBluetoothPowerState, true),

      Log("Waiting for toggle to be checked again"),
      WaitForToggleState(kOsSettingsElementId, kBluetoothToggleQuery, true),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
