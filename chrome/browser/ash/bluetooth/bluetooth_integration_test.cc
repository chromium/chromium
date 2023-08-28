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
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "dbus/object_path.h"
#include "device/bluetooth/dbus/bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/floss/floss_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace ash {
namespace {

using bluez::BluetoothAdapterClient;
using bluez::BluezDBusManager;

// State tracker for Bluetooth power-on.
class BluetoothPowerStateObserver : public ui::test::ObservationStateObserver<
                                        bool,
                                        BluetoothAdapterClient,
                                        BluetoothAdapterClient::Observer> {
 public:
  BluetoothPowerStateObserver(BluetoothAdapterClient* adapter_client,
                              BluetoothAdapterClient::Properties* properties)
      : ObservationStateObserver(adapter_client), properties_(properties) {}

  ~BluetoothPowerStateObserver() override = default;

  // ui::test::ObservationStateObserver:
  bool GetStateObserverInitialState() const override {
    return properties_->powered.value();
  }

  // BluetoothAdapterClient::Observer:
  void AdapterPropertyChanged(const dbus::ObjectPath& object_path,
                              const std::string& property_name) override {
    if (property_name == properties_->powered.name()) {
      OnStateObserverStateChanged(properties_->powered.value());
    }
  }

 private:
  const raw_ptr<BluetoothAdapterClient::Properties> properties_;
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(BluetoothPowerStateObserver,
                                    kBluetoothPowerState);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kButtonToggled);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExists);

constexpr char kBluetoothDevicesSubpagePath[] = "bluetoothDevices";
constexpr char kCheckJsElementIsChecked[] = "(el) => { return el.checked; }";
constexpr char kCheckJsElementIsNotChecked[] =
    "(el) => { return !el.checked; }";

class BluetoothIntegrationTest : public InteractiveAshTest {
 public:
  BluetoothIntegrationTest() {
    // Use the legacy bluez bluetooth stack.
    feature_list_.InitAndDisableFeature(floss::features::kFlossEnabled);
  }

  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    bluez_dbus_manager_ = BluezDBusManager::Get();
    if (!bluez_dbus_manager_) {
      // TODO(crbug.com/1464750): Come up with a better way to skip tests based
      // on hardware support, similar to Tast hwdep.D(hwdep.Bluetooth()).
      LOG(WARNING) << "Bluetooth (via bluez) not supported on this device.";
      GTEST_SKIP();
    }

    // Get the D-Bus property tracker for the first bluetooth adapter.
    adapter_client_ = bluez_dbus_manager_->GetBluetoothAdapterClient();
    ASSERT_TRUE(adapter_client_);
    std::vector<dbus::ObjectPath> adapters = adapter_client_->GetAdapters();
    // Some VM images have bluez, but no bluetooth adapters.
    if (adapters.empty()) {
      LOG(WARNING) << "No bluetooth adapters, skipping test";
      GTEST_SKIP();
    }
    properties_ = adapter_client_->GetProperties(adapters[0]);
    ASSERT_TRUE(properties_);
  }

  void TearDownOnMainThread() override {
    // Avoid dangling pointers during shutdown.
    properties_ = nullptr;
    adapter_client_ = nullptr;
    bluez_dbus_manager_ = nullptr;

    InteractiveAshTest::TearDownOnMainThread();
  }

  // Waits for an element to exist in the DOM.
  auto WaitForElementExists(const ui::ElementIdentifier& element_id,
                            const DeepQuery& query) {
    StateChange element_exists;
    element_exists.event = kElementExists;
    element_exists.where = query;
    return WaitForStateChange(element_id, element_exists);
  }

  // Waits for a toggle element to be toggled (which is represented as "checked"
  // in the DOM).
  auto WaitForToggleState(const ui::ElementIdentifier& element_id,
                          DeepQuery element,
                          bool is_checked) {
    StateChange toggle_selection_change;
    toggle_selection_change.event = kButtonToggled;
    toggle_selection_change.where = element;
    toggle_selection_change.type = StateChange::Type::kExistsAndConditionTrue;
    toggle_selection_change.test_function =
        is_checked ? kCheckJsElementIsChecked : kCheckJsElementIsNotChecked;

    return WaitForStateChange(element_id, toggle_selection_change);
  }

  // Clicks on an element in the DOM.
  auto ClickElement(const ui::ElementIdentifier& element_id,
                    const DeepQuery& element) {
    return Steps(MoveMouseTo(element_id, element), ClickMouse());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<BluezDBusManager> bluez_dbus_manager_ = nullptr;
  raw_ptr<BluetoothAdapterClient> adapter_client_ = nullptr;
  raw_ptr<BluetoothAdapterClient::Properties> properties_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(BluetoothIntegrationTest,
                       ToggleBluetoothFromQuickSettings) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-ba8a2ed6-be82-4174-bf38-489eba7181c6");
  SetupContextWidget();
  RunTestSequence(ObserveState(kBluetoothPowerState,
                               std::make_unique<BluetoothPowerStateObserver>(
                                   adapter_client_, properties_)),
                  // Ensure bluetooth is on at test start. If this fails it
                  // means some previous test left bluetooth disabled.
                  WaitForState(kBluetoothPowerState, true),

                  Log("Opening quick settings bubble"),
                  PressButton(kUnifiedSystemTrayElementId),
                  WaitForShow(kQuickSettingsViewElementId),

                  // The bluetooth toggle button may take time to enable because
                  // the UI queries the bluetooth adapter state asynchronously.
                  Log("Waiting for bluetooth toggle button to enable"),
                  WaitForViewProperty(kBluetoothFeatureTileToggleElementId,
                                      views::View, Enabled, true),

                  Log("Pressing bluetooth toggle button"),
                  PressButton(kBluetoothFeatureTileToggleElementId),

                  Log("Waiting for bluetooth adapter to power off"),
                  WaitForState(kBluetoothPowerState, false),

                  // Allow UI state to settle.
                  FlushEvents(),

                  Log("Pressing bluetooth toggle button again"),
                  PressButton(kBluetoothFeatureTileToggleElementId),

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
      // Ensure bluetooth is on at test start. If this fails it
      // means some previous test left bluetooth disabled.
      Log("Verifying initial bluetooth power state"),
      ObserveState(kBluetoothPowerState,
                   std::make_unique<BluetoothPowerStateObserver>(
                       adapter_client_, properties_)),
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
