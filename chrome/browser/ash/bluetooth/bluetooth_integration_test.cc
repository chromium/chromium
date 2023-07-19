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
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "dbus/object_path.h"
#include "device/bluetooth/dbus/bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/floss/floss_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
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
  const base::raw_ptr<BluetoothAdapterClient::Properties> properties_;
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(BluetoothPowerStateObserver,
                                    kBluetoothPowerState);

// Give all widgets on the same display (having the same root window) the same
// Kombucha context. This is useful for ash system UI because it uses a variety
// of small widgets.
ui::ElementContext GetContextForWidget(views::Widget* widget) {
  return ui::ElementContext(widget->GetNativeWindow()->GetRootWindow());
}

class BluetoothIntegrationTest : public InteractiveBrowserTest {
 public:
  BluetoothIntegrationTest() {
    // Use the legacy bluez bluetooth stack.
    feature_list_.InitAndDisableFeature(floss::features::kFlossEnabled);

    // This test suite does not require a browser window.
    set_launch_browser_for_testing(nullptr);

    // Give all widgets on the same display the same Kombucha context.
    views::ElementTrackerViews::SetContextOverrideCallback(
        base::BindRepeating(&GetContextForWidget));
  }

  ~BluetoothIntegrationTest() override {
    views::ElementTrackerViews::SetContextOverrideCallback({});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(BluetoothIntegrationTest,
                       ToggleBluetoothFromQuickSettings) {
  // TODO(crbug.com/1464750): Come up with a better way to skip tests based on
  // hardware support, similar to Tast hwdep.D(hwdep.Bluetooth()).
  if (!BluezDBusManager::Get()) {
    LOG(WARNING) << "Bluetooth (via bluez) not supported on this device.";
    GTEST_SKIP();
  }

  // Get the D-Bus property tracker for the first bluetooth adapter.
  auto* bluez_dbus_manager = BluezDBusManager::Get();
  ASSERT_TRUE(bluez_dbus_manager);
  auto* adapter_client = bluez_dbus_manager->GetBluetoothAdapterClient();
  ASSERT_TRUE(adapter_client);
  std::vector<dbus::ObjectPath> adapters = adapter_client->GetAdapters();
  ASSERT_FALSE(adapters.empty());
  auto* properties = adapter_client->GetProperties(adapters[0]);
  ASSERT_TRUE(properties);

  // Kombucha requires a context widget to synthesize clicks.
  views::Widget* status_area_widget =
      Shell::GetPrimaryRootWindowController()->shelf()->GetStatusAreaWidget();
  SetContextWidget(status_area_widget);

  RunTestSequence(ObserveState(kBluetoothPowerState,
                               std::make_unique<BluetoothPowerStateObserver>(
                                   adapter_client, properties)),
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

}  // namespace
}  // namespace ash
