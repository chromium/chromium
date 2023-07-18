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
#include "ui/views/interaction/element_tracker_views.h"

namespace ash {
namespace {

using bluez::BluetoothAdapterClient;
using bluez::BluezDBusManager;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBluetoothPoweredElementId);

// A fake UI element that shows itself when the bluetooth adapter is powered on
// and hides itself when the adapter is powered off. It tracks the OS bluetooth
// daemon state via D-Bus properties. This element allows bluetooth states to be
// detected by Kombucha WaitForShow() / WaitForHide().
class BluetoothPoweredElement : public ui::test::TestElement,
                                public BluetoothAdapterClient::Observer {
 public:
  BluetoothPoweredElement(ui::ElementContext context,
                          BluetoothAdapterClient* adapter_client,
                          BluetoothAdapterClient::Properties* properties)
      : ui::test::TestElement(kBluetoothPoweredElementId, context),
        adapter_client_(
            raw_ref<BluetoothAdapterClient>::from_ptr(adapter_client)),
        properties_(
            raw_ref<BluetoothAdapterClient::Properties>::from_ptr(properties)) {
    adapter_client_->AddObserver(this);
    // Show the element immediately if the adapter is powered.
    if (properties_->powered.value()) {
      Show();
    }
  }

  ~BluetoothPoweredElement() override { adapter_client_->RemoveObserver(this); }

  // BluetoothAdapterClient::Observer:
  void AdapterPropertyChanged(const dbus::ObjectPath& object_path,
                              const std::string& property_name) override {
    if (property_name == properties_->powered.name()) {
      if (properties_->powered.value()) {
        Show();
      } else {
        Hide();
      }
    }
  }

 private:
  const raw_ref<BluetoothAdapterClient> adapter_client_;
  const raw_ref<BluetoothAdapterClient::Properties> properties_;
};

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

  // A more readable way to wait for the bluetooth adapter to be powered on.
  auto WaitForBluetoothPoweredOn() {
    return WaitForShow(kBluetoothPoweredElementId);
  }

  // A more readable way to wait for the bluetooth adapter to be powered off.
  auto WaitForBluetoothPoweredOff() {
    return WaitForHide(kBluetoothPoweredElementId);
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

  // Create the fake element representing bluetooth adapter state from D-Bus.
  // Use the same Kombucha context as the rest of the test.
  ui::ElementContext context = GetContextForWidget(status_area_widget);
  BluetoothPoweredElement bluetooth_powered_element(context, adapter_client,
                                                    properties);

  RunTestSequence(
      // Ensure bluetooth is on at test start. If this fails it means some
      // previous test left bluetooth disabled.
      WaitForBluetoothPoweredOn(),

      Log("Opening quick settings bubble"),
      PressButton(kUnifiedSystemTrayElementId),
      WaitForShow(kQuickSettingsViewElementId),

      // The bluetooth toggle button may take time to enable because the UI
      // queries the bluetooth adapter state asynchronously.
      Log("Waiting for bluetooth toggle button to enable"),
      WaitForViewProperty(kBluetoothFeatureTileToggleElementId, views::View,
                          Enabled, true),

      Log("Pressing bluetooth toggle button"),
      PressButton(kBluetoothFeatureTileToggleElementId),

      Log("Waiting for bluetooth adapter to power off"),
      WaitForBluetoothPoweredOff(),

      // Allow UI state to settle.
      FlushEvents(),

      Log("Pressing bluetooth toggle button again"),
      PressButton(kBluetoothFeatureTileToggleElementId),

      Log("Waiting for bluetooth adapter to power on"),
      WaitForBluetoothPoweredOn());
}

}  // namespace
}  // namespace ash
