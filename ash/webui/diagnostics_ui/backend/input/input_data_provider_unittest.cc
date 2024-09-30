// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/diagnostics_ui/backend/input/input_data_provider.h"

#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "ash/system/diagnostics/diagnostics_log_controller.h"
#include "ash/system/diagnostics/fake_diagnostics_browser_delegate.h"
#include "ash/system/diagnostics/keyboard_input_log.h"
#include "ash/system/diagnostics/log_test_helpers.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/diagnostics_ui/backend/input/event_watcher_factory.h"
#include "ash/webui/diagnostics_ui/backend/input/input_data_event_watcher.h"
#include "ash/webui/diagnostics_ui/backend/input/keyboard_input_data_event_watcher.h"
#include "ash/webui/diagnostics_ui/mojom/input_data_provider.mojom.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "content/public/test/browser_task_environment.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/events/ash/event_rewriter_ash.h"
#include "ui/events/ash/fake_event_rewriter_ash_delegate.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touch_device_transform.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/ozone/device/device_event_observer.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

// Note: this is not a recommended pattern, but works and allows cleanly
// formatted invocations for this test set.
#define EXPECT_KEY_EVENTS(observerptr, id, ...)    \
  do {                                             \
    SCOPED_TRACE("EXPECT_KEY_EVENTS invocation");  \
    ExpectKeyEvents(observerptr, id, __VA_ARGS__); \
  } while (0);

namespace ash {
namespace diagnostics {

namespace {

constexpr mojom::TopRowKey kClassicTopRowKeys[] = {
    mojom::TopRowKey::kBack,
    mojom::TopRowKey::kForward,
    mojom::TopRowKey::kRefresh,
    mojom::TopRowKey::kFullscreen,
    mojom::TopRowKey::kOverview,
    mojom::TopRowKey::kScreenBrightnessDown,
    mojom::TopRowKey::kScreenBrightnessUp,
    mojom::TopRowKey::kVolumeMute,
    mojom::TopRowKey::kVolumeDown,
    mojom::TopRowKey::kVolumeUp};

const std::vector<uint32_t> kInternalJinlonScanCodes = {
    0xEA, 0xE7, 0x91, 0x92, 0x93, 0x94, 0x95,
    0x96, 0x97, 0x98, 0xA0, 0xAE, 0xB0};

constexpr mojom::TopRowKey kInternalJinlonTopRowKeys[] = {
    mojom::TopRowKey::kBack,
    mojom::TopRowKey::kRefresh,
    mojom::TopRowKey::kFullscreen,
    mojom::TopRowKey::kOverview,
    mojom::TopRowKey::kScreenshot,
    mojom::TopRowKey::kScreenBrightnessDown,
    mojom::TopRowKey::kScreenBrightnessUp,
    mojom::TopRowKey::kPrivacyScreenToggle,
    mojom::TopRowKey::kKeyboardBacklightDown,
    mojom::TopRowKey::kKeyboardBacklightUp,
    mojom::TopRowKey::kVolumeMute,
    mojom::TopRowKey::kVolumeDown,
    mojom::TopRowKey::kVolumeUp};

constexpr ui::TopRowActionKey kInternalJinlonActionKeys[] = {
    ui::TopRowActionKey::kBack,
    ui::TopRowActionKey::kRefresh,
    ui::TopRowActionKey::kFullscreen,
    ui::TopRowActionKey::kOverview,
    ui::TopRowActionKey::kScreenshot,
    ui::TopRowActionKey::kScreenBrightnessDown,
    ui::TopRowActionKey::kScreenBrightnessUp,
    ui::TopRowActionKey::kPrivacyScreenToggle,
    ui::TopRowActionKey::kKeyboardBacklightDown,
    ui::TopRowActionKey::kKeyboardBacklightUp,
    ui::TopRowActionKey::kVolumeMute,
    ui::TopRowActionKey::kVolumeDown,
    ui::TopRowActionKey::kVolumeUp};

// One possible variant of a Dell configuration
constexpr mojom::TopRowKey kInternalDellTopRowKeys[] = {
    mojom::TopRowKey::kBack,
    mojom::TopRowKey::kRefresh,
    mojom::TopRowKey::kFullscreen,
    mojom::TopRowKey::kOverview,
    mojom::TopRowKey::kScreenBrightnessDown,
    mojom::TopRowKey::kScreenBrightnessUp,
    mojom::TopRowKey::kVolumeMute,
    mojom::TopRowKey::kVolumeDown,
    mojom::TopRowKey::kVolumeUp,
    mojom::TopRowKey::kNone,
    mojom::TopRowKey::kNone,
    mojom::TopRowKey::kScreenMirror,
    mojom::TopRowKey::kDelete};

constexpr char kKbdTopRowPropertyName[] = "CROS_KEYBOARD_TOP_ROW_LAYOUT";
constexpr char kKbdTopRowLayoutAttributeName[] = "function_row_physmap";

constexpr char kSillyDeviceName[] = "eventWithoutANumber";

constexpr char kInvalidMechnicalLayout[] = "Not ANSI, JIS, or ISO";

// Privacy Screen replaced with unknown 0xC4 scancode.
constexpr char kModifiedJinlonDescriptor[] =
    "EA E7 91 92 93 94 95 C4 97 98 A0 AE B0";
constexpr uint32_t kUnknownScancode = 0xC4;
constexpr int kUnknownScancodeIndex = 7;

// Device id in DeviceDataManager.
constexpr int kDeviceId1 = 1;
constexpr int kDeviceId2 = 2;

struct KeyDefinition {
  uint32_t key_code;
  uint32_t at_scan_code;
  uint32_t usb_scan_code;
};

// TODO(b/211780758): we should acquire these tuples from dom_code_data.inc,
// where feasible.
constexpr KeyDefinition kKeyA = {KEY_A, 0x1E, 0x70004};
constexpr KeyDefinition kKeyB = {KEY_B, 0x30, 0x70005};
constexpr KeyDefinition kKeyEsc = {KEY_ESC, 0x30, 0x70005};
constexpr KeyDefinition kKeyF1 = {KEY_F1, 0x3B, 0x7003A};
constexpr KeyDefinition kKeyF8 = {KEY_F8, 0x42, 0x70041};
constexpr KeyDefinition kKeyF10 = {KEY_F10, 0x44, 0x70043};
// Drallion AT codes for F11-F12; not standardized
constexpr KeyDefinition kKeyF11 = {KEY_F11, 0x57, 0x700044};
constexpr KeyDefinition kKeyF12 = {KEY_F12, 0xD7, 0x700045};
constexpr KeyDefinition kKeyDelete = {KEY_DELETE, 0xD3, 0x7004C};
// Eve AT code; unknown if this is standard
constexpr KeyDefinition kKeyMenu = {KEY_CONTROLPANEL, 0x5D, 0};
// Jinlon AT code; unknown if this is standard
constexpr KeyDefinition kKeySleep = {KEY_SLEEP, 0x5D, 0};
constexpr KeyDefinition kKeyActionBack = {KEY_BACK, 0xEA, 0x0C0224};
constexpr KeyDefinition kKeyActionRefresh = {KEY_REFRESH, 0xE7, 0x0C0227};
constexpr KeyDefinition kKeyActionFullscreen = {KEY_ZOOM, 0x91, 0x0C0232};
constexpr KeyDefinition kKeyActionOverview = {KEY_SCALE, 0x92, 0x0C029F};
constexpr KeyDefinition kKeyActionScreenshot = {KEY_SYSRQ, 0x93, 0x070046};
constexpr KeyDefinition kKeyActionScreenBrightnessDown = {KEY_BRIGHTNESSDOWN,
                                                          0x94, 0x0C0070};
constexpr KeyDefinition kKeyActionScreenBrightnessUp = {KEY_BRIGHTNESSUP, 0x95,
                                                        0x0C006F};
constexpr KeyDefinition kKeyActionKeyboardBrightnessDown = {KEY_KBDILLUMDOWN,
                                                            0x97, 0x0C007A};
constexpr KeyDefinition kKeyActionKeyboardBrightnessUp = {KEY_KBDILLUMUP, 0x98,
                                                          0x0C0079};
constexpr KeyDefinition kKeyActionKeyboardVolumeMute = {KEY_MUTE, 0xA0,
                                                        0x0C00E2};
constexpr KeyDefinition kKeyActionKeyboardVolumeDown = {KEY_VOLUMEDOWN, 0xAE,
                                                        0x0C00EA};
constexpr KeyDefinition kKeyActionKeyboardVolumeUp = {KEY_VOLUMEUP, 0xB0,
                                                      0x0C00E9};

constexpr uint32_t kKeyboardTesterMetricTimeDelay = 10u;
#if 0
// TODO(b/208729519): Not useful until we can test Drallion keyboards.
// Drallion, no HID equivalent
constexpr KeyDefinition kKeySwitchVideoMode = {KEY_SWITCHVIDEOMODE, 0x8B, 0};
constexpr KeyDefinition kKeyActionPrivacyScreenToggle =
   {KEY_PRIVACY_SCREEN_TOGGLE, 0x96, 0x0C02D0};
#endif

// NOTE: This is only creates a simple ui::InputDevice based on a device
// capabilities report; it is not suitable for subclasses of ui::InputDevice.
ui::InputDevice InputDeviceFromCapabilities(
    int device_id,
    const ui::DeviceCapabilities& capabilities) {
  ui::EventDeviceInfo device_info = {};
  ui::CapabilitiesToDeviceInfo(capabilities, &device_info);

  const std::string sys_path =
      base::StringPrintf("/dev/input/event%d-%s", device_id, capabilities.path);

  return ui::InputDevice(device_id, device_info.device_type(),
                         device_info.name(), device_info.phys(),
                         base::FilePath(sys_path), device_info.vendor_id(),
                         device_info.product_id(), device_info.version());
}

}  // namespace

namespace mojom {

std::ostream& operator<<(std::ostream& os, const KeyEvent& event) {
  os << "KeyEvent{ id=" << event.id << ", ";
  os << "type=" << event.type << ", ";
  os << "key_code=" << event.key_code << ", ";
  os << "scan_code=" << event.scan_code << ", ";
  os << "top_row_position=" << event.top_row_position;
  os << "}";
  return os;
}

}  // namespace mojom

// Fake device manager that lets us control the input devices that
// an InputDataProvider can see.
class FakeDeviceManager : public ui::DeviceManager {
 public:
  FakeDeviceManager() {}
  FakeDeviceManager(const FakeDeviceManager&) = delete;
  FakeDeviceManager& operator=(const FakeDeviceManager&) = delete;
  ~FakeDeviceManager() override {}

  // DeviceManager:
  void ScanDevices(ui::DeviceEventObserver* observer) override {}
  void AddObserver(ui::DeviceEventObserver* observer) override {}
  void RemoveObserver(ui::DeviceEventObserver* observer) override {}
};

class FakeInputDataEventWatcher;
typedef std::map<uint32_t, raw_ptr<FakeInputDataEventWatcher, CtnExperimental>>
    watchers_t;

// Fake evdev watcher class that lets us manually post input
// events into an InputDataProvider; this keeps an external
// map of watchers updated so that instances can easily be found.
class FakeInputDataEventWatcher : public InputDataEventWatcher {
 public:
  FakeInputDataEventWatcher(
      uint32_t id,
      base::WeakPtr<KeyboardInputDataEventWatcher::Dispatcher> dispatcher,
      watchers_t& watchers)
      : InputDataEventWatcher(id),
        dispatcher_(dispatcher),
        watchers_(watchers) {
    EXPECT_EQ(0u, watchers_->count(this->evdev_id_));
    (*watchers_)[this->evdev_id_] = this;
  }
  ~FakeInputDataEventWatcher() override {
    EXPECT_EQ((*watchers_)[this->evdev_id_], this);
    watchers_->erase(this->evdev_id_);
  }

  void PostKeyEvent(bool down, uint32_t evdev_code, uint32_t scan_code) {
    if (dispatcher_)
      dispatcher_->SendInputKeyEvent(this->evdev_id_, evdev_code, scan_code,
                                     down);
  }

  // ProcessEvent will not be triggered by test code.
  void ProcessEvent(const input_event& event) override {}

  // Only updating boolean instead of watching actual FD. FD watch tested in
  // InputDataEventWatcher unit tests.
  // See: ash/webui/diagnostics_ui/backend/input_data_event_watcher_unittest.cc
  void DoStart() override {}
  void DoStop() override {}

 private:
  base::WeakPtr<KeyboardInputDataEventWatcher::Dispatcher> dispatcher_;
  const raw_ref<watchers_t> watchers_;
};

// Utility to construct FakeInputDataEventWatcher for InputDataProvider.
class FakeInputDataEventWatcherFactory : public EventWatcherFactory {
 public:
  explicit FakeInputDataEventWatcherFactory(watchers_t& watchers)
      : watchers_(watchers) {}
  FakeInputDataEventWatcherFactory(const FakeInputDataEventWatcherFactory&) =
      delete;
  FakeInputDataEventWatcherFactory& operator=(
      const FakeInputDataEventWatcherFactory&) = delete;
  ~FakeInputDataEventWatcherFactory() override = default;

  std::unique_ptr<InputDataEventWatcher> MakeKeyboardEventWatcher(
      uint32_t id,
      base::WeakPtr<KeyboardInputDataEventWatcher::Dispatcher> dispatcher)
      override {
    return std::make_unique<FakeInputDataEventWatcher>(
        id, std::move(dispatcher), *watchers_);
  }

 private:
  const raw_ref<watchers_t> watchers_;
};

// A mock observer that records device change events emitted from an
// InputDataProvider.
class FakeConnectedDevicesObserver : public mojom::ConnectedDevicesObserver {
 public:
  // mojom::ConnectedDevicesObserver:
  void OnTouchDeviceConnected(
      mojom::TouchDeviceInfoPtr new_touch_device) override {
    touch_devices_connected.push_back(std::move(new_touch_device));
  }
  void OnTouchDeviceDisconnected(uint32_t id) override {
    touch_devices_disconnected.push_back(id);
  }
  void OnKeyboardConnected(mojom::KeyboardInfoPtr new_keyboard) override {
    keyboards_connected.push_back(std::move(new_keyboard));
  }
  void OnKeyboardDisconnected(uint32_t id) override {
    keyboards_disconnected.push_back(id);
  }

  std::vector<mojom::TouchDeviceInfoPtr> touch_devices_connected;
  std::vector<uint32_t> touch_devices_disconnected;
  std::vector<mojom::KeyboardInfoPtr> keyboards_connected;
  std::vector<uint32_t> keyboards_disconnected;

  mojo::Receiver<mojom::ConnectedDevicesObserver> receiver{this};
};

// A mock observer that records key event events emitted from an
// InputDataProvider.
class FakeKeyboardObserver : public mojom::KeyboardObserver {
 public:
  enum EventType {
    kEvent = 1,
    kPause = 2,
    kResume = 3,
  };

  // mojom::KeyboardObserver:
  void OnKeyEvent(mojom::KeyEventPtr key_event) override {
    events_.push_back({kEvent, std::move(key_event)});
  }
  void OnKeyEventsPaused() override { events_.push_back({kPause, nullptr}); }
  void OnKeyEventsResumed() override { events_.push_back({kResume, nullptr}); }

  std::vector<std::pair<EventType, mojom::KeyEventPtr>> events_;

  mojo::Receiver<mojom::KeyboardObserver> receiver{this};
};

// A mock observer that records current tablet mode status and counts when
// OnTabletModeChanged function is called.
class FakeTabletModeObserver : public mojom::TabletModeObserver {
 public:
  uint32_t num_tablet_mode_change_calls() const {
    return num_tablet_mode_change_calls_;
  }

  bool is_tablet_mode() { return is_tablet_mode_; }

  // mojom::TabletModeObserver:
  void OnTabletModeChanged(bool is_tablet_mode) override {
    ++num_tablet_mode_change_calls_;
    is_tablet_mode_ = is_tablet_mode;
  }

  mojo::Receiver<mojom::TabletModeObserver> receiver{this};

 private:
  uint32_t num_tablet_mode_change_calls_ = 0;
  bool is_tablet_mode_ = false;
};

class FakeLidStateObserver : public mojom::LidStateObserver {
 public:
  uint32_t num_lid_state_change_calls() const {
    return num_lid_state_change_calls_;
  }

  bool is_lid_open() { return is_lid_open_; }

  // mojom::TabletModeObserver:
  void OnLidStateChanged(bool is_lid_open) override {
    ++num_lid_state_change_calls_;
    is_lid_open_ = is_lid_open;
  }

  mojo::Receiver<mojom::LidStateObserver> receiver{this};

 private:
  uint32_t num_lid_state_change_calls_ = 0;
  bool is_lid_open_ = false;
};

// A mock observer that records current internal display power state and counts
// when OnInternalDisplayPowerStateChanged function is called.
class FakeInternalDisplayPowerStateObserver
    : public mojom::InternalDisplayPowerStateObserver {
 public:
  uint32_t num_display_state_change_calls() const {
    return num_display_state_change_calls_;
  }

  bool is_display_on() { return is_display_on_; }

  // mojom::InternalDisplayPowerStateObserver:
  void OnInternalDisplayPowerStateChanged(bool is_display_on) override {
    ++num_display_state_change_calls_;
    is_display_on_ = is_display_on;
  }

  mojo::Receiver<mojom::InternalDisplayPowerStateObserver> receiver{this};

 private:
  uint32_t num_display_state_change_calls_ = 0;
  bool is_display_on_ = true;
};

// A utility class that fakes obtaining information about an evdev.
class FakeInputDeviceInfoHelper : public InputDeviceInfoHelper {
 public:
  FakeInputDeviceInfoHelper() {}

  ~FakeInputDeviceInfoHelper() override {}

  std::unique_ptr<InputDeviceInformation> GetDeviceInfo(
      int id,
      base::FilePath path) override {
    ui::DeviceCapabilities device_caps;
    const std::string base_name = path.BaseName().value();
    auto info = std::make_unique<InputDeviceInformation>();
    std::unique_ptr<ui::KeyboardCapability::KeyboardInfo> keyboard_info;

    if (base_name == "event0") {
      device_caps = ui::kLinkKeyboard;
      info->keyboard_type =
          ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
      info->keyboard_top_row_layout =
          ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout1;
      EXPECT_EQ(0, id);
    } else if (base_name == "event1") {
      device_caps = ui::kLinkTouchpad;
      EXPECT_EQ(1, id);
    } else if (base_name == "event2") {
      device_caps = ui::kKohakuTouchscreen;
      EXPECT_EQ(2, id);
    } else if (base_name == "event3") {
      device_caps = ui::kKohakuStylus;
      EXPECT_EQ(3, id);
    } else if (base_name == "event4") {
      device_caps = ui::kHpUsbKeyboard;
      info->keyboard_type =
          ui::KeyboardCapability::DeviceType::kDeviceExternalGenericKeyboard;
      info->keyboard_top_row_layout =
          ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutDefault;
      EXPECT_EQ(4, id);
    } else if (base_name == "event5") {
      device_caps = ui::kSarienKeyboard;  // Wilco
      info->keyboard_type =
          ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
      info->keyboard_top_row_layout =
          ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutWilco;
      EXPECT_EQ(5, id);
    } else if (base_name == "event6") {
      device_caps = ui::kEveKeyboard;
      info->keyboard_type =
          ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
      info->keyboard_top_row_layout =
          ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout2;
      EXPECT_EQ(6, id);
    } else if (base_name == "event7") {
      device_caps = ui::kJinlonKeyboard;
      info->keyboard_type =
          ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
      info->keyboard_top_row_layout =
          ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom;
      info->keyboard_scan_codes = kInternalJinlonScanCodes;

      keyboard_info = std::make_unique<ui::KeyboardCapability::KeyboardInfo>();
      keyboard_info->device_type =
          ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
      keyboard_info->top_row_action_keys.assign(
          std::begin(kInternalJinlonActionKeys),
          std::end(kInternalJinlonActionKeys));
      keyboard_info->top_row_layout =
          ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom;
      keyboard_info->top_row_scan_codes = kInternalJinlonScanCodes;
      EXPECT_EQ(7, id);
    } else if (base_name == "event8") {
      device_caps = ui::kMicrosoftBluetoothNumberPad;
      info->keyboard_type =
          ui::KeyboardCapability::DeviceType::kDeviceExternalGenericKeyboard;
      info->keyboard_top_row_layout =
          ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutDefault;
      EXPECT_EQ(8, id);
    } else if (base_name == "event9") {
      device_caps = ui::kLogitechTouchKeyboardK400;
      info->keyboard_type =
          ui::KeyboardCapability::DeviceType::kDeviceExternalGenericKeyboard;
      info->keyboard_top_row_layout =
          ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutDefault;
      EXPECT_EQ(9, id);
    } else if (base_name == "event10") {
      device_caps = ui::kDrallionKeyboard;
      EXPECT_EQ(10, id);
    } else if (base_name == "event11") {
      // Used for customized top row layout.
      device_caps = ui::kJinlonKeyboard;
      device_caps.kbd_function_row_physmap = kModifiedJinlonDescriptor;
      info->keyboard_type =
          ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
      info->keyboard_top_row_layout =
          ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom;
      info->keyboard_scan_codes = kInternalJinlonScanCodes;
      // Set 0xC4 to be F8.
      info->keyboard_scan_codes[7] = 0xC4;

      keyboard_info = std::make_unique<ui::KeyboardCapability::KeyboardInfo>();
      keyboard_info->device_type =
          ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
      keyboard_info->top_row_action_keys.assign(
          std::begin(kInternalJinlonActionKeys),
          std::end(kInternalJinlonActionKeys));
      keyboard_info->top_row_layout =
          ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom;
      keyboard_info->top_row_scan_codes = kInternalJinlonScanCodes;
      keyboard_info->top_row_scan_codes[7] = 0xC4;
      keyboard_info->top_row_action_keys[7] = ui::TopRowActionKey::kUnknown;
      EXPECT_EQ(11, id);
    } else if (base_name == "event12") {
      device_caps = ui::kMorphiusTabletModeSwitch;
      EXPECT_EQ(12, id);
    } else if (base_name == "event13") {
      device_caps = ui::kHammerKeyboard;
      info->keyboard_type =
          ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
      info->keyboard_top_row_layout =
          ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout2;
      EXPECT_EQ(13, id);
    } else if (base_name == "event14") {
      device_caps = ui::kBaskingTouchScreen;
      EXPECT_EQ(14, id);
    } else if (base_name == "event15") {
      device_caps = ui::kSplitModifierKeyboard;
      info->keyboard_type =
          ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
      info->keyboard_top_row_layout =
          ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom;
      EXPECT_EQ(15, id);
    } else if (base_name == kSillyDeviceName) {
      // Simulate a device that is properly described, but has a malformed
      // device name.
      EXPECT_EQ(98, id);
      device_caps = ui::kLinkKeyboard;
    } else if (base_name == "event99") {
      EXPECT_EQ(99, id);
      // Simulate a device that couldn't be opened or have its info determined
      // for whatever reason.
      return nullptr;
    }

    EXPECT_TRUE(
        ui::CapabilitiesToDeviceInfo(device_caps, &info->event_device_info));
    info->evdev_id = id;
    info->path = path;
    info->input_device =
        InputDeviceFromCapabilities(info->evdev_id, device_caps);
    info->connection_type =
        InputDataProvider::ConnectionTypeFromInputDeviceType(
            info->event_device_info.device_type());

    if (keyboard_info) {
      Shell::Get()
          ->keyboard_capability()
          ->DisableKeyboardInfoTrimmingForTesting();
      Shell::Get()->keyboard_capability()->SetKeyboardInfoForTesting(
          ui::KeyboardDevice(info->input_device), std::move(*keyboard_info));
    }

    return info;
  }
};

// Our modifications to InputDataProvider that carries around its own
// widget (representing the window that needs to be visible for key events
// to be observed), the needed factories for our fake utilities, and a
// reference to the current event watchers.
class TestInputDataProvider : public InputDataProvider {
 public:
  TestInputDataProvider(views::Widget* widget,
                        watchers_t& watchers,
                        ui::EventRewriterAsh::Delegate* event_rewriter_delegate)
      : InputDataProvider(
            widget->GetNativeWindow(),
            std::make_unique<FakeDeviceManager>(),
            std::make_unique<FakeInputDataEventWatcherFactory>(watchers),
            Shell::Get()->accelerator_controller(),
            event_rewriter_delegate),
        attached_widget_(widget),
        watchers_(watchers) {
    info_helper_ = base::SequenceBound<FakeInputDeviceInfoHelper>(
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
  }
  explicit TestInputDataProvider(const TestInputDataProvider&) = delete;
  TestInputDataProvider& operator=(const TestInputDataProvider&) = delete;

  // The widget represents the tab that input diagnostics would normally be
  // shown in. This is allocated outside this class so it won't
  // be destroyed early. (See next item.)
  raw_ptr<views::Widget> attached_widget_;
  // Keep a list of watchers for each evdev in the provider. This is a
  // reference to an instance outside of this class, as the lifetime of the
  // list needs to exceed the destruction of this test class, and can only be
  // cleaned up once all watchers have been destroyed by the base
  // InputDataProvider, which occurs after our destruction.
  const raw_ref<watchers_t> watchers_;
};

class InputDataProviderTest : public AshTestBase {
 public:
  InputDataProviderTest()
      : AshTestBase(content::BrowserTaskEnvironment::TimeSource::MOCK_TIME) {}

  InputDataProviderTest(const InputDataProviderTest&) = delete;
  InputDataProviderTest& operator=(const InputDataProviderTest&) = delete;
  ~InputDataProviderTest() override = default;

  void SetUp() override {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndEnableFeature(
        features::kEnableExternalKeyboardsInDiagnostics);

    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();

    // Note: some init for creating widgets is performed in base SetUp
    // instead of the constructor, so our init must also be delayed until
    // SetUp, so we can safely invoke CreateTestWidget().

    statistics_provider_.SetMachineStatistic(
        system::kKeyboardMechanicalLayoutKey, "ANSI");
    system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    fake_udev_ = std::make_unique<testing::FakeUdevLoader>();
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    provider_ = std::make_unique<TestInputDataProvider>(
        widget_.get(), watchers_, &event_rewriter_delegate_);
    DiagnosticsLogController::Initialize(
        std::make_unique<FakeDiagnosticsBrowserDelegate>());

    // Apply these early, in SetUp; delaying until
    // FakeInputDeviceInfoHelper::GetDeviceInfo() is not appropriate, as
    // fake_udev is not thread safe. (If multiple devices are constructed in a
    // row, then GetDeviceInfo() invocation can overlap with
    // ProcessInputDataProvider::ProcessDeviceInfo() which reads from udev).
    UdevAddFakeDeviceCapabilities("/dev/input/event5", ui::kSarienKeyboard);
    UdevAddFakeDeviceCapabilities("/dev/input/event6", ui::kEveKeyboard);
    UdevAddFakeDeviceCapabilities("/dev/input/event7", ui::kJinlonKeyboard);
    UdevAddFakeDeviceCapabilities("/dev/input/event10", ui::kDrallionKeyboard);
    // Tweak top row keys for event11.
    auto device_caps = ui::kJinlonKeyboard;
    device_caps.kbd_function_row_physmap = kModifiedJinlonDescriptor;
    UdevAddFakeDeviceCapabilities("/dev/input/event11", device_caps);
  }

  void TearDown() override {
    provider_.reset();
    base::RunLoop().RunUntilIdle();
    AshTestBase::TearDown();
  }

  bool OpenAndCloseLauncher() {
    const auto launcher_accelerator =
        ui::Accelerator(ui::VKEY_ALL_APPLICATIONS, ui::EF_NONE,
                        ui::Accelerator::KeyState::PRESSED);

    // Open and close the launcher
    return Shell::Get()->accelerator_controller()->AcceleratorPressed(
               launcher_accelerator) &&
           Shell::Get()->accelerator_controller()->AcceleratorPressed(
               launcher_accelerator);
  }

  bool ModifierRewritesAreSuppressed() {
    return !event_rewriter_delegate_.RewriteModifierKeys();
  }

 protected:
  struct ExpectedKeyEvent {
    KeyDefinition key;
    int position;
    bool down = true;
  };

  void ExpectKeyEvents(FakeKeyboardObserver* fake_observer,
                       uint32_t id,
                       std::initializer_list<ExpectedKeyEvent> list) {
    // Make sure the test does something...
    EXPECT_TRUE(std::size(list) > 0);

    size_t i;

    i = 0;
    for (auto* iter = list.begin(); iter != list.end(); iter++, i++) {
      (*provider_->watchers_)[id]->PostKeyEvent(iter->down, iter->key.key_code,
                                                iter->key.at_scan_code);
    }
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(std::size(list), fake_observer->events_.size());

    i = 0;
    for (auto* iter = list.begin(); iter != list.end(); iter++, i++) {
      EXPECT_EQ(
          *fake_observer->events_[i].second,
          mojom::KeyEvent(/*id=*/id,
                          /*type=*/iter->down ? mojom::KeyEventType::kPress
                                              : mojom::KeyEventType::kRelease,
                          /*key_code=*/iter->key.key_code,
                          /*scan_code=*/iter->key.at_scan_code,
                          /*top_row_position=*/iter->position))
          << " which is EXPECT_KEY_EVENTS item #" << i;
    }
  }

  void UdevAddFakeDeviceCapabilities(
      const std::string& device_name,
      const ui::DeviceCapabilities& device_caps) {
    std::map<std::string, std::string>
        sysfs_properties;  // Old style numeric tags.
    std::map<std::string, std::string>
        sysfs_attributes;  // New style vivaldi scancode layouts.

    if (device_caps.kbd_function_row_physmap &&
        strlen(device_caps.kbd_function_row_physmap) > 0) {
      sysfs_attributes[kKbdTopRowLayoutAttributeName] =
          device_caps.kbd_function_row_physmap;
    }

    if (device_caps.kbd_top_row_layout &&
        strlen(device_caps.kbd_top_row_layout) > 0) {
      sysfs_properties[kKbdTopRowPropertyName] = device_caps.kbd_top_row_layout;
    }

    // Each device needs a unique sys path; many of the ones embedded in
    // capabilities are the same, so uniquify them with the event device name.
    // These aren't actual valid paths, but nothing in the testing logic needs
    // them to be real.
    const std::string sys_path = device_name + "-" + device_caps.path;

    fake_udev_->AddFakeDevice(device_caps.name, sys_path.c_str(),
                              /*subsystem=*/"input", /*devnode=*/std::nullopt,
                              /*devtype=*/std::nullopt,
                              std::move(sysfs_attributes),
                              std::move(sysfs_properties));
  }

  ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
  std::unique_ptr<testing::FakeUdevLoader> fake_udev_;
  system::FakeStatisticsProvider statistics_provider_;
  std::unique_ptr<views::Widget> widget_;
  // All evdev watchers in use by provider_.
  watchers_t watchers_;
  ui::test::FakeEventRewriterAshDelegate event_rewriter_delegate_;
  std::unique_ptr<TestInputDataProvider> provider_;

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

TEST_F(InputDataProviderTest, GetConnectedDevices_DeviceInfoMapping) {
  ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event0"));
  ui::DeviceEvent event1(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event1"));
  ui::DeviceEvent event2(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event2"));
  ui::DeviceEvent event3(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event3"));
  provider_->OnDeviceEvent(event0);
  provider_->OnDeviceEvent(event1);
  provider_->OnDeviceEvent(event2);
  provider_->OnDeviceEvent(event3);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());

  const auto& keyboards = future.Get<0>();
  const auto& touch_devices = future.Get<1>();

  ASSERT_EQ(1ul, keyboards.size());
  // The stylus device should be filtered out, hence only 2 touch devices.
  ASSERT_EQ(2ul, touch_devices.size());

  const mojom::KeyboardInfoPtr& keyboard = keyboards[0];
  EXPECT_EQ(0u, keyboard->id);
  EXPECT_EQ(mojom::ConnectionType::kInternal, keyboard->connection_type);
  EXPECT_EQ("AT Translated Set 2 keyboard", keyboard->name);

  const mojom::TouchDeviceInfoPtr& touchpad = touch_devices[0];
  EXPECT_EQ(1u, touchpad->id);
  EXPECT_EQ(mojom::ConnectionType::kInternal, touchpad->connection_type);
  EXPECT_EQ(mojom::TouchDeviceType::kPointer, touchpad->type);
  EXPECT_EQ("Atmel maXTouch Touchpad", touchpad->name);

  const mojom::TouchDeviceInfoPtr& touchscreen = touch_devices[1];
  EXPECT_EQ(2u, touchscreen->id);
  EXPECT_EQ(mojom::ConnectionType::kInternal, touchscreen->connection_type);
  EXPECT_EQ(mojom::TouchDeviceType::kDirect, touchscreen->type);
  EXPECT_EQ("Atmel maXTouch Touchscreen", touchscreen->name);
}

TEST_F(InputDataProviderTest, GetConnectedDevices_HasInternalKeyboard) {
  // Initialize one internal keyboard in DeviceDataManager.
  std::vector<ui::KeyboardDevice> keyboard_devices;
  keyboard_devices.push_back(
      ui::KeyboardDevice(kDeviceId1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                         "Internal Keyboard"));
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(keyboard_devices);

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());

  // The return values are supposed to be not ready since GetConnectedDevices()
  // function will wait for the internal keyboard to be added.
  ASSERT_FALSE(future.IsReady());

  // Add an internal keyboard.
  ui::DeviceEvent event(ui::DeviceEvent::DeviceType::INPUT,
                        ui::DeviceEvent::ActionType::ADD,
                        base::FilePath("/dev/input/event5"));
  provider_->OnDeviceEvent(event);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(future.IsReady());
  const auto& keyboards = future.Get<0>();
  ASSERT_EQ(1ul, keyboards.size());
}

TEST_F(InputDataProviderTest, GetConnectedDevices_SplitModifierKeyboard) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kModifierSplit);
  auto ignore_modifier_split_secret_key =
      ash::switches::SetIgnoreModifierSplitSecretKeyForTest();

  Shell::Get()
      ->keyboard_capability()
      ->ResetModifierSplitDogfoodControllerForTesting();

  // Initialize one split modifier keyboard in DeviceDataManager.
  std::vector<ui::KeyboardDevice> keyboard_devices;
  keyboard_devices.emplace_back(
      kDeviceId1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      "Split Modifier Keyboard", /*has_assistant_key=*/true,
      /*has_function_key=*/true);
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(keyboard_devices);

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());

  // The return values are supposed to be ready since GetConnectedDevices()
  // function won't wait for the split modifier keyboard to be added.
  ASSERT_TRUE(future.IsReady());
}

TEST_F(InputDataProviderTest, FilterOutSplitModifierKeyboard) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kModifierSplit);
  auto ignore_modifier_split_secret_key =
      ash::switches::SetIgnoreModifierSplitSecretKeyForTest();

  Shell::Get()
      ->keyboard_capability()
      ->ResetModifierSplitDogfoodControllerForTesting();

  // Initialize one split modifier keyboard in DeviceDataManager.
  std::vector<ui::KeyboardDevice> keyboard_devices;
  keyboard_devices.emplace_back(
      kDeviceId1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      "Split Modifier Keyboard", /*has_assistant_key=*/true,
      /*has_function_key=*/true);
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(keyboard_devices);

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());

  // Add an split modifier keyboard.
  ui::DeviceEvent event(ui::DeviceEvent::DeviceType::INPUT,
                        ui::DeviceEvent::ActionType::ADD,
                        base::FilePath("/dev/input/event15"));
  provider_->OnDeviceEvent(event);
  base::RunLoop().RunUntilIdle();

  const auto& keyboards = future.Get<0>();
  ASSERT_EQ(0ul, keyboards.size());
}

TEST_F(InputDataProviderTest, GetConnectedDevices_AddEventAfterFirstCall) {
  {
    base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                           std::vector<mojom::TouchDeviceInfoPtr>>
        future;
    provider_->GetConnectedDevices(future.GetCallback());

    const auto& keyboards = future.Get<0>();
    const auto& touch_devices = future.Get<1>();
    ASSERT_EQ(0ul, keyboards.size());
    ASSERT_EQ(0ul, touch_devices.size());
  }

  ui::DeviceEvent event(ui::DeviceEvent::DeviceType::INPUT,
                        ui::DeviceEvent::ActionType::ADD,
                        base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(event);
  base::RunLoop().RunUntilIdle();

  {
    base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                           std::vector<mojom::TouchDeviceInfoPtr>>
        future;
    provider_->GetConnectedDevices(future.GetCallback());

    const auto& keyboards = future.Get<0>();
    const auto& touch_devices = future.Get<1>();

    ASSERT_EQ(1ul, keyboards.size());
    const mojom::KeyboardInfoPtr& keyboard = keyboards[0];
    EXPECT_EQ(4u, keyboard->id);
    EXPECT_EQ(mojom::ConnectionType::kUsb, keyboard->connection_type);
    EXPECT_EQ("Chicony HP Elite USB Keyboard", keyboard->name);

    EXPECT_EQ(0ul, touch_devices.size());
  }
}

TEST_F(InputDataProviderTest, GetConnectedDevices_AddUnusualDevices) {
  // Add two devices with unusual bus types, and verify connection types.

  ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event8"));
  ui::DeviceEvent event1(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event9"));
  provider_->OnDeviceEvent(event0);
  provider_->OnDeviceEvent(event1);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());

  const auto& keyboards = future.Get<0>();
  const auto& touch_devices = future.Get<1>();

  ASSERT_EQ(2ul, keyboards.size());
  ASSERT_EQ(0ul, touch_devices.size());

  const mojom::KeyboardInfoPtr& keyboard1 = keyboards[0];
  EXPECT_EQ(8u, keyboard1->id);
  EXPECT_EQ(mojom::ConnectionType::kBluetooth, keyboard1->connection_type);
  EXPECT_EQ(ui::kMicrosoftBluetoothNumberPad.name, keyboard1->name);

  const mojom::KeyboardInfoPtr& keyboard2 = keyboards[1];
  EXPECT_EQ(9u, keyboard2->id);
  EXPECT_EQ(mojom::ConnectionType::kUnknown, keyboard2->connection_type);
  EXPECT_EQ(ui::kLogitechTouchKeyboardK400.name, keyboard2->name);
}

TEST_F(InputDataProviderTest, GetConnectedDevices_Remove) {
  ui::DeviceEvent add_touch_event(ui::DeviceEvent::DeviceType::INPUT,
                                  ui::DeviceEvent::ActionType::ADD,
                                  base::FilePath("/dev/input/event1"));
  provider_->OnDeviceEvent(add_touch_event);
  ui::DeviceEvent add_kbd_event(ui::DeviceEvent::DeviceType::INPUT,
                                ui::DeviceEvent::ActionType::ADD,
                                base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(add_kbd_event);
  base::RunLoop().RunUntilIdle();

  {
    base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                           std::vector<mojom::TouchDeviceInfoPtr>>
        future;
    provider_->GetConnectedDevices(future.GetCallback());

    const auto& keyboards = future.Get<0>();
    const auto& touch_devices = future.Get<1>();

    ASSERT_EQ(1ul, keyboards.size());
    EXPECT_EQ(4u, keyboards[0]->id);

    ASSERT_EQ(1ul, touch_devices.size());
    EXPECT_EQ(1u, touch_devices[0]->id);
  }

  ui::DeviceEvent remove_touch_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::REMOVE,
                                     base::FilePath("/dev/input/event1"));
  provider_->OnDeviceEvent(remove_touch_event);
  ui::DeviceEvent remove_kbd_event(ui::DeviceEvent::DeviceType::INPUT,
                                   ui::DeviceEvent::ActionType::REMOVE,
                                   base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(remove_kbd_event);
  base::RunLoop().RunUntilIdle();

  {
    base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                           std::vector<mojom::TouchDeviceInfoPtr>>
        future;
    provider_->GetConnectedDevices(future.GetCallback());

    const auto& keyboards = future.Get<0>();
    const auto& touch_devices = future.Get<1>();

    EXPECT_EQ(0ul, keyboards.size());
    EXPECT_EQ(0ul, touch_devices.size());
  }
}

TEST_F(InputDataProviderTest, GetConnectedDevices_NoExternalKeyboards) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kEnableExternalKeyboardsInDiagnostics);

  ui::DeviceEvent add_internal_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::ADD,
                                     base::FilePath("/dev/input/event0"));
  ui::DeviceEvent add_external_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::ADD,
                                     base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(add_internal_event);
  provider_->OnDeviceEvent(add_external_event);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());

  const auto& keyboards = future.Get<0>();

  ASSERT_EQ(1ul, keyboards.size());

  const mojom::KeyboardInfoPtr& internal_kbd = keyboards[0];
  EXPECT_EQ(0u, internal_kbd->id);
  EXPECT_EQ(mojom::ConnectionType::kInternal, internal_kbd->connection_type);
}

TEST_F(InputDataProviderTest, KeyboardPhysicalLayoutDetection) {
  statistics_provider_.SetMachineStatistic(system::kKeyboardMechanicalLayoutKey,
                                           "ISO");

  ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event0"));
  ui::DeviceEvent event1(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event4"));
  ui::DeviceEvent event2(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event5"));
  ui::DeviceEvent event3(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event7"));
  provider_->OnDeviceEvent(event0);
  provider_->OnDeviceEvent(event1);
  provider_->OnDeviceEvent(event2);
  provider_->OnDeviceEvent(event3);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());

  const auto& keyboards = future.Get<0>();

  ASSERT_EQ(4ul, keyboards.size());

  const mojom::KeyboardInfoPtr& builtin_keyboard = keyboards[0];
  EXPECT_EQ(0u, builtin_keyboard->id);
  EXPECT_EQ(mojom::PhysicalLayout::kChromeOS,
            builtin_keyboard->physical_layout);
  EXPECT_EQ(mojom::MechanicalLayout::kIso, builtin_keyboard->mechanical_layout);
  EXPECT_EQ(mojom::NumberPadPresence::kNotPresent,
            builtin_keyboard->number_pad_present);
  EXPECT_EQ(
      std::vector(std::begin(kClassicTopRowKeys), std::end(kClassicTopRowKeys)),
      builtin_keyboard->top_row_keys);

  const mojom::KeyboardInfoPtr& external_keyboard = keyboards[1];
  EXPECT_EQ(4u, external_keyboard->id);
  EXPECT_EQ(mojom::PhysicalLayout::kUnknown,
            external_keyboard->physical_layout);
  EXPECT_EQ(mojom::MechanicalLayout::kUnknown,
            external_keyboard->mechanical_layout);
  EXPECT_EQ(mojom::NumberPadPresence::kUnknown,
            external_keyboard->number_pad_present);
  EXPECT_EQ(
      std::vector(std::begin(kClassicTopRowKeys), std::end(kClassicTopRowKeys)),
      external_keyboard->top_row_keys);

  const mojom::KeyboardInfoPtr& dell_internal_keyboard = keyboards[2];
  EXPECT_EQ(5u, dell_internal_keyboard->id);
  EXPECT_EQ(mojom::PhysicalLayout::kChromeOSDellEnterpriseWilco,
            dell_internal_keyboard->physical_layout);
  EXPECT_EQ(mojom::MechanicalLayout::kIso,
            dell_internal_keyboard->mechanical_layout);
  EXPECT_EQ(mojom::NumberPadPresence::kNotPresent,
            dell_internal_keyboard->number_pad_present);
  EXPECT_EQ(std::vector(std::begin(kInternalDellTopRowKeys),
                        std::end(kInternalDellTopRowKeys)),
            dell_internal_keyboard->top_row_keys);

  const mojom::KeyboardInfoPtr& jinlon_internal_keyboard = keyboards[3];
  EXPECT_EQ(7u, jinlon_internal_keyboard->id);
  EXPECT_EQ(mojom::PhysicalLayout::kChromeOS,
            jinlon_internal_keyboard->physical_layout);
  EXPECT_EQ(mojom::MechanicalLayout::kIso,
            jinlon_internal_keyboard->mechanical_layout);
  EXPECT_EQ(mojom::NumberPadPresence::kNotPresent,
            jinlon_internal_keyboard->number_pad_present);
  EXPECT_EQ(std::vector(std::begin(kInternalJinlonTopRowKeys),
                        std::end(kInternalJinlonTopRowKeys)),
            jinlon_internal_keyboard->top_row_keys);

  // TODO(b/208729519): We should check a Drallion keyboard, however that
  // invokes a check through the global Shell that does not operate in
  // this test.
}

TEST_F(InputDataProviderTest, KeyboardRegionDetection) {
  statistics_provider_.SetMachineStatistic(system::kRegionKey, "jp");

  ui::DeviceEvent event_internal(ui::DeviceEvent::DeviceType::INPUT,
                                 ui::DeviceEvent::ActionType::ADD,
                                 base::FilePath("/dev/input/event0"));
  ui::DeviceEvent event_external(ui::DeviceEvent::DeviceType::INPUT,
                                 ui::DeviceEvent::ActionType::ADD,
                                 base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(event_internal);
  provider_->OnDeviceEvent(event_external);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());

  const auto& keyboards = future.Get<0>();

  ASSERT_EQ(2ul, keyboards.size());

  const mojom::KeyboardInfoPtr& internal_keyboard = keyboards[0];
  EXPECT_EQ("jp", internal_keyboard->region_code);

  const mojom::KeyboardInfoPtr& external_keyboard = keyboards[1];
  EXPECT_EQ(std::nullopt, external_keyboard->region_code);
}

TEST_F(InputDataProviderTest, KeyboardRegionDetection_Failure) {
  statistics_provider_.ClearMachineStatistic(system::kRegionKey);

  ui::DeviceEvent event_internal(ui::DeviceEvent::DeviceType::INPUT,
                                 ui::DeviceEvent::ActionType::ADD,
                                 base::FilePath("/dev/input/event0"));
  provider_->OnDeviceEvent(event_internal);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());

  const auto& keyboards = future.Get<0>();

  ASSERT_EQ(1ul, keyboards.size());

  const mojom::KeyboardInfoPtr& internal_keyboard = keyboards[0];
  EXPECT_EQ(std::nullopt, internal_keyboard->region_code);
}

TEST_F(InputDataProviderTest, KeyboardAssistantKeyDetection) {
  ui::DeviceEvent link_event(ui::DeviceEvent::DeviceType::INPUT,
                             ui::DeviceEvent::ActionType::ADD,
                             base::FilePath("/dev/input/event0"));
  ui::DeviceEvent eve_event(ui::DeviceEvent::DeviceType::INPUT,
                            ui::DeviceEvent::ActionType::ADD,
                            base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(link_event);
  provider_->OnDeviceEvent(eve_event);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());
  const auto& keyboards = future.Get<0>();

  ASSERT_EQ(2ul, keyboards.size());

  const mojom::KeyboardInfoPtr& link_keyboard = keyboards[0];
  EXPECT_EQ(0u, link_keyboard->id);
  EXPECT_FALSE(link_keyboard->has_assistant_key);
  const mojom::KeyboardInfoPtr& eve_keyboard = keyboards[1];
  EXPECT_EQ(6u, eve_keyboard->id);
  EXPECT_TRUE(eve_keyboard->has_assistant_key);
}

TEST_F(InputDataProviderTest, KeyboardNumberPadDetectionInternal) {
  // Detection of internal number pad depends on command-line
  // argument, and is not a property of the keyboard device.

  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--has-number-pad"});
  ui::DeviceEvent link_event(ui::DeviceEvent::DeviceType::INPUT,
                             ui::DeviceEvent::ActionType::ADD,
                             base::FilePath("/dev/input/event0"));
  provider_->OnDeviceEvent(link_event);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());
  const auto& keyboards = future.Get<0>();

  ASSERT_EQ(1ul, keyboards.size());

  const mojom::KeyboardInfoPtr& builtin_keyboard = keyboards[0];
  EXPECT_EQ(0u, builtin_keyboard->id);
  EXPECT_EQ(mojom::NumberPadPresence::kPresent,
            builtin_keyboard->number_pad_present);
}

TEST_F(InputDataProviderTest, KeyboardTopRightKey_Clamshell) {
  // Devices without a tablet mode switch should be assumed to be clamshells,
  // with a power key in the top-right.
  ui::DeviceEvent event_keyboard(ui::DeviceEvent::DeviceType::INPUT,
                                 ui::DeviceEvent::ActionType::ADD,
                                 base::FilePath("/dev/input/event0"));
  provider_->OnDeviceEvent(event_keyboard);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());

  const auto& keyboards = future.Get<0>();

  ASSERT_EQ(1ul, keyboards.size());

  const mojom::KeyboardInfoPtr& keyboard = keyboards[0];
  EXPECT_EQ(mojom::TopRightKey::kPower, keyboard->top_right_key);
}

TEST_F(InputDataProviderTest, KeyboardTopRightKey_Convertible_ModeSwitchFirst) {
  // Devices with a tablet mode switch should be assumed to be convertibles,
  // with a lock key in the top-right.
  ui::DeviceEvent event_mode_switch(ui::DeviceEvent::DeviceType::INPUT,
                                    ui::DeviceEvent::ActionType::ADD,
                                    base::FilePath("/dev/input/event12"));
  ui::DeviceEvent event_keyboard(ui::DeviceEvent::DeviceType::INPUT,
                                 ui::DeviceEvent::ActionType::ADD,
                                 base::FilePath("/dev/input/event11"));
  provider_->OnDeviceEvent(event_mode_switch);
  provider_->OnDeviceEvent(event_keyboard);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());

  const auto& keyboards = future.Get<0>();

  ASSERT_EQ(1ul, keyboards.size());

  const mojom::KeyboardInfoPtr& keyboard = keyboards[0];
  EXPECT_EQ(mojom::TopRightKey::kLock, keyboard->top_right_key);
}

TEST_F(InputDataProviderTest, KeyboardTopRightKey_Convertible_KeyboardFirst) {
  // Devices with a tablet mode switch should be assumed to be convertibles,
  // with a lock key in the top-right, even if we get the tablet mode switch
  // event after the keyboard event.
  ui::DeviceEvent event_keyboard(ui::DeviceEvent::DeviceType::INPUT,
                                 ui::DeviceEvent::ActionType::ADD,
                                 base::FilePath("/dev/input/event11"));
  ui::DeviceEvent event_mode_switch(ui::DeviceEvent::DeviceType::INPUT,
                                    ui::DeviceEvent::ActionType::ADD,
                                    base::FilePath("/dev/input/event12"));
  provider_->OnDeviceEvent(event_keyboard);
  provider_->OnDeviceEvent(event_mode_switch);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());

  const auto& keyboards = future.Get<0>();

  ASSERT_EQ(1ul, keyboards.size());

  const mojom::KeyboardInfoPtr& keyboard = keyboards[0];
  EXPECT_EQ(mojom::TopRightKey::kLock, keyboard->top_right_key);
}

TEST_F(InputDataProviderTest, KeyboardTopRightKey_Detachable) {
  // "Internal" keyboards which are connected by USB are actually detachable,
  // and therefore should be assumed to have a lock key in the top-right.
  ui::DeviceEvent event_keyboard(ui::DeviceEvent::DeviceType::INPUT,
                                 ui::DeviceEvent::ActionType::ADD,
                                 base::FilePath("/dev/input/event13"));
  provider_->OnDeviceEvent(event_keyboard);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());

  const auto& keyboards = future.Get<0>();

  ASSERT_EQ(1ul, keyboards.size());

  const mojom::KeyboardInfoPtr& keyboard = keyboards[0];
  EXPECT_EQ(mojom::TopRightKey::kLock, keyboard->top_right_key);
}

TEST_F(InputDataProviderTest, ObserveConnectedDevices_Keyboards) {
  FakeConnectedDevicesObserver fake_observer;
  provider_->ObserveConnectedDevices(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  ui::DeviceEvent add_keyboard_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::ADD,
                                     base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(add_keyboard_event);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1ul, fake_observer.keyboards_connected.size());
  EXPECT_EQ(4u, fake_observer.keyboards_connected[0]->id);

  ui::DeviceEvent remove_keyboard_event(ui::DeviceEvent::DeviceType::INPUT,
                                        ui::DeviceEvent::ActionType::REMOVE,
                                        base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(remove_keyboard_event);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1ul, fake_observer.keyboards_disconnected.size());
  EXPECT_EQ(4u, fake_observer.keyboards_disconnected[0]);
}

TEST_F(InputDataProviderTest, ObserveConnectedDevices_TouchDevices) {
  FakeConnectedDevicesObserver fake_observer;
  provider_->ObserveConnectedDevices(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  ui::DeviceEvent add_touch_event(ui::DeviceEvent::DeviceType::INPUT,
                                  ui::DeviceEvent::ActionType::ADD,
                                  base::FilePath("/dev/input/event1"));
  provider_->OnDeviceEvent(add_touch_event);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1ul, fake_observer.touch_devices_connected.size());
  EXPECT_EQ(1u, fake_observer.touch_devices_connected[0]->id);

  ui::DeviceEvent remove_touch_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::REMOVE,
                                     base::FilePath("/dev/input/event1"));
  provider_->OnDeviceEvent(remove_touch_event);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1ul, fake_observer.touch_devices_disconnected.size());
  EXPECT_EQ(1u, fake_observer.touch_devices_disconnected[0]);
}

TEST_F(InputDataProviderTest, ObserveConnectedDevices_NoExternalKeyboards) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kEnableExternalKeyboardsInDiagnostics);

  FakeConnectedDevicesObserver fake_observer;
  provider_->ObserveConnectedDevices(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  ui::DeviceEvent add_external_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::ADD,
                                     base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(add_external_event);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0ul, fake_observer.keyboards_connected.size());

  ui::DeviceEvent remove_external_event(ui::DeviceEvent::DeviceType::INPUT,
                                        ui::DeviceEvent::ActionType::REMOVE,
                                        base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(remove_external_event);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0ul, fake_observer.keyboards_disconnected.size());
}

TEST_F(InputDataProviderTest, ChangeDeviceDoesNotCrash) {
  ui::DeviceEvent add_device_event(ui::DeviceEvent::DeviceType::INPUT,
                                   ui::DeviceEvent::ActionType::ADD,
                                   base::FilePath("/dev/input/event1"));
  ui::DeviceEvent change_device_event(ui::DeviceEvent::DeviceType::INPUT,
                                      ui::DeviceEvent::ActionType::CHANGE,
                                      base::FilePath("/dev/input/event1"));
  provider_->OnDeviceEvent(add_device_event);
  base::RunLoop().RunUntilIdle();
  provider_->OnDeviceEvent(change_device_event);
  base::RunLoop().RunUntilIdle();
}

TEST_F(InputDataProviderTest, BadDeviceDoesNotCrash) {
  // Try a device that specifically fails to be processed.
  ui::DeviceEvent add_bad_device_event(ui::DeviceEvent::DeviceType::INPUT,
                                       ui::DeviceEvent::ActionType::ADD,
                                       base::FilePath("/dev/input/event99"));
  provider_->OnDeviceEvent(add_bad_device_event);
  base::RunLoop().RunUntilIdle();
}

TEST_F(InputDataProviderTest, SillyDeviceDoesNotCrash) {
  // Try a device that has data, but has a non-parseable name.
  ui::DeviceEvent add_silly_device_event(ui::DeviceEvent::DeviceType::INPUT,
                                         ui::DeviceEvent::ActionType::ADD,
                                         base::FilePath(kSillyDeviceName));
  provider_->OnDeviceEvent(add_silly_device_event);
  base::RunLoop().RunUntilIdle();
}

TEST_F(InputDataProviderTest, GetKeyboardMechanicalLayout_Unknown1) {
  statistics_provider_.ClearMachineStatistic(
      system::kKeyboardMechanicalLayoutKey);

  ui::DeviceEvent add_keyboard_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::ADD,
                                     base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(add_keyboard_event);
  base::RunLoop().RunUntilIdle();

  {
    base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                           std::vector<mojom::TouchDeviceInfoPtr>>
        future;
    provider_->GetConnectedDevices(future.GetCallback());

    const auto& keyboards = future.Get<0>();

    ASSERT_EQ(1ul, keyboards.size());

    const mojom::KeyboardInfoPtr& builtin_keyboard = keyboards[0];
    EXPECT_EQ(6u, builtin_keyboard->id);
    EXPECT_EQ(mojom::PhysicalLayout::kChromeOS,
              builtin_keyboard->physical_layout);
    EXPECT_EQ(mojom::MechanicalLayout::kUnknown,
              builtin_keyboard->mechanical_layout);
    EXPECT_EQ(mojom::NumberPadPresence::kNotPresent,
              builtin_keyboard->number_pad_present);
  }
}

TEST_F(InputDataProviderTest, GetKeyboardMechanicalLayout_Unknown2) {
  statistics_provider_.SetMachineStatistic(system::kKeyboardMechanicalLayoutKey,
                                           kInvalidMechnicalLayout);
  ui::DeviceEvent add_keyboard_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::ADD,
                                     base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(add_keyboard_event);
  base::RunLoop().RunUntilIdle();

  {
    base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                           std::vector<mojom::TouchDeviceInfoPtr>>
        future;
    provider_->GetConnectedDevices(future.GetCallback());

    const auto& keyboards = future.Get<0>();

    ASSERT_EQ(1ul, keyboards.size());

    const mojom::KeyboardInfoPtr& builtin_keyboard = keyboards[0];
    EXPECT_EQ(6u, builtin_keyboard->id);
    EXPECT_EQ(mojom::PhysicalLayout::kChromeOS,
              builtin_keyboard->physical_layout);
    EXPECT_EQ(mojom::MechanicalLayout::kUnknown,
              builtin_keyboard->mechanical_layout);
    EXPECT_EQ(mojom::NumberPadPresence::kNotPresent,
              builtin_keyboard->number_pad_present);
  }
}

TEST_F(InputDataProviderTest, ResetReceiverOnBindInterface) {
  // This test simulates a user refreshing the WebUI page. The receiver should
  // be reset before binding the new receiver. Otherwise we would get a DCHECK
  // error from mojo::Receiver
  mojo::Remote<mojom::InputDataProvider> remote;
  provider_->BindInterface(remote.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();

  remote.reset();

  provider_->BindInterface(remote.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();
}

TEST_F(InputDataProviderTest, KeyObservationBasic) {
  std::unique_ptr<FakeKeyboardObserver> fake_observer =
      std::make_unique<FakeKeyboardObserver>();

  // Widget must be active and visible.
  provider_->attached_widget_->Show();
  provider_->attached_widget_->Activate();

  // Construct a keyboard.
  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, fake_observer->events_.size());
  EXPECT_EQ(0u, provider_->watchers_->size());

  // Attach a key observer.
  provider_->ObserveKeyEvents(
      6u, fake_observer->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Ensure an event watcher was constructed for the observer,
  // but has not posted any events.
  EXPECT_EQ(0u, fake_observer->events_.size());
  EXPECT_EQ(1u, provider_->watchers_->size());
  ASSERT_TRUE((*provider_->watchers_)[6]);

  // Post a key event through the watcher that
  // was created for the observer.
  (*provider_->watchers_)[6]->PostKeyEvent(true, kKeyA.key_code,
                                           kKeyA.at_scan_code);
  base::RunLoop().RunUntilIdle();

  // Ensure the event came through.
  EXPECT_EQ(1u, fake_observer->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kEvent, fake_observer->events_[0].first);
  ASSERT_TRUE(fake_observer->events_[0].second);

  EXPECT_EQ(*fake_observer->events_[0].second,
            mojom::KeyEvent(/*id=*/6u, /*type=*/mojom::KeyEventType::kPress,
                            /*key_code=*/kKeyA.key_code,
                            /*scan_code=*/kKeyA.at_scan_code,
                            /*top_row_position=*/-1));
}

TEST_F(InputDataProviderTest, KeyObservationRemoval) {
  std::unique_ptr<FakeKeyboardObserver> fake_observer =
      std::make_unique<FakeKeyboardObserver>();

  // Widget must be active and visible.
  provider_->attached_widget_->Show();
  provider_->attached_widget_->Activate();

  // Construct a keyboard.
  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, fake_observer->events_.size());
  EXPECT_EQ(0u, provider_->watchers_->size());

  bool disconnected = false;

  // Attach a key observer.
  provider_->ObserveKeyEvents(
      6u, fake_observer->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  fake_observer->receiver.set_disconnect_handler(
      base::BindOnce([](bool* disconnected) { *disconnected = true; },
                     base::Unretained(&disconnected)));

  base::RunLoop().RunUntilIdle();

  // Ensure an event watcher was constructed for the observer,
  // but has not posted any events.
  EXPECT_EQ(0u, fake_observer->events_.size());
  EXPECT_EQ(1u, provider_->watchers_->size());
  EXPECT_FALSE(disconnected);
  ASSERT_TRUE((*provider_->watchers_)[6]);

  // Test a key event.
  EXPECT_KEY_EVENTS(fake_observer.get(), 6u, {{kKeyA, -1}});

  // Disconnect keyboard while it is being observed.
  ui::DeviceEvent remove_kbd_event(ui::DeviceEvent::DeviceType::INPUT,
                                   ui::DeviceEvent::ActionType::REMOVE,
                                   base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(remove_kbd_event);
  base::RunLoop().RunUntilIdle();

  // Watcher should have been shut down, and receiver disconnected.
  EXPECT_FALSE((*provider_->watchers_)[6]);
  EXPECT_TRUE(disconnected);
}

TEST_F(InputDataProviderTest, KeyObservationMultiple) {
  std::unique_ptr<FakeKeyboardObserver> fake_observer =
      std::make_unique<FakeKeyboardObserver>();

  // Widget must be active and visible.
  provider_->attached_widget_->Show();
  provider_->attached_widget_->Activate();

  // Construct a keyboard.
  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  // Attach a key observer.
  provider_->ObserveKeyEvents(
      6u, fake_observer->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE((*provider_->watchers_)[6]);

  EXPECT_KEY_EVENTS(fake_observer.get(), 6u,
                    {{kKeyA, -1, true},
                     {kKeyB, -1, true},
                     {kKeyA, -1, false},
                     {kKeyB, -1, false}});
}

TEST_F(InputDataProviderTest, KeyObservationObeysFocus) {
  std::unique_ptr<FakeKeyboardObserver> fake_observer =
      std::make_unique<FakeKeyboardObserver>();

  provider_->attached_widget_->Deactivate();
  provider_->attached_widget_->Hide();

  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  provider_->ObserveKeyEvents(
      6u, fake_observer->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Verify we got the pause event from hiding the window.
  ASSERT_EQ(1u, fake_observer->events_.size());
  ASSERT_TRUE((*provider_->watchers_)[6]);

  EXPECT_EQ(FakeKeyboardObserver::kPause, fake_observer->events_[0].first);

  // Post a key event through the watcher that
  // was created for the observer.
  (*provider_->watchers_)[6]->PostKeyEvent(true, kKeyA.key_code,
                                           kKeyA.at_scan_code);
  base::RunLoop().RunUntilIdle();

  // Ensure the event did not come through, as the widget was not visible and
  // focused.
  ASSERT_EQ(1u, fake_observer->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kPause, fake_observer->events_[0].first);
}

TEST_F(InputDataProviderTest, KeyObservationDisconnect) {
  std::unique_ptr<FakeKeyboardObserver> fake_observer =
      std::make_unique<FakeKeyboardObserver>();

  // Widget must be active and visible.
  provider_->attached_widget_->Show();
  provider_->attached_widget_->Activate();

  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  provider_->ObserveKeyEvents(
      6u, fake_observer->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, fake_observer->events_.size());
  ASSERT_TRUE((*provider_->watchers_)[6]);

  fake_observer->receiver.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, fake_observer->events_.size());
  ASSERT_FALSE((*provider_->watchers_)[6]);
}

TEST_F(InputDataProviderTest, KeyObservationObeysFocusSwitching) {
  std::unique_ptr<FakeKeyboardObserver> fake_observer =
      std::make_unique<FakeKeyboardObserver>();
  std::unique_ptr<views::Widget> other_widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  // Provider's widget must be active and visible.
  provider_->attached_widget_->Show();

  // Construct a keyboard.
  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, fake_observer->events_.size());
  EXPECT_EQ(0u, provider_->watchers_->size());

  // Attach a key observer.
  provider_->ObserveKeyEvents(
      6u, fake_observer->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Ensure an event watcher was constructed for the observer,
  // but has not posted any events.
  EXPECT_EQ(0u, fake_observer->events_.size());
  EXPECT_EQ(1u, provider_->watchers_->size());
  ASSERT_TRUE((*provider_->watchers_)[6]);

  // Focus on the other window.
  other_widget->Show();
  other_widget->Activate();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(provider_->attached_widget_->IsActive());
  EXPECT_TRUE(other_widget->IsVisible());
  EXPECT_TRUE(other_widget->IsActive());

  EXPECT_EQ(1u, fake_observer->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kPause, fake_observer->events_[0].first);
  ASSERT_FALSE(fake_observer->events_[0].second);

  // Post a key event through the watcher that
  // was created for the observer.
  (*provider_->watchers_)[6]->PostKeyEvent(true, kKeyA.key_code,
                                           kKeyA.at_scan_code);
  base::RunLoop().RunUntilIdle();

  // Ensure the event did not come through.
  EXPECT_EQ(1u, fake_observer->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kPause, fake_observer->events_[0].first);

  // Clear events for next round.
  fake_observer->events_.clear();

  // Switch windows back.
  provider_->attached_widget_->Show();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(provider_->attached_widget_->IsActive());
  EXPECT_FALSE(other_widget->IsActive());

  // Post another key event.
  (*provider_->watchers_)[6]->PostKeyEvent(true, kKeyB.key_code,
                                           kKeyB.at_scan_code);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, fake_observer->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kResume, fake_observer->events_[0].first);
  EXPECT_EQ(FakeKeyboardObserver::kEvent, fake_observer->events_[1].first);
  ASSERT_TRUE(fake_observer->events_[1].second);

  EXPECT_EQ(*fake_observer->events_[1].second,
            mojom::KeyEvent(/*id=*/6u, /*type=*/mojom::KeyEventType::kPress,
                            /*key_code=*/kKeyB.key_code,
                            /*scan_code=*/kKeyB.at_scan_code,
                            /*top_row_position=*/-1));
}

TEST_F(InputDataProviderTest, ShortcutBlockingObeysFocus) {
  const std::string kDevicePath("/dev/input/event6");
  const uint32_t kDeviceId = 6u;

  std::unique_ptr<FakeKeyboardObserver> fake_observer =
      std::make_unique<FakeKeyboardObserver>();

  provider_->attached_widget_->Deactivate();
  provider_->attached_widget_->Hide();

  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath(kDevicePath));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(OpenAndCloseLauncher());
  EXPECT_FALSE(ModifierRewritesAreSuppressed());
  EXPECT_TRUE(InputDataProvider::ShouldCloseDialogOnEscape());

  // If widget is in focus, ObserveKeyEvents should block shortcuts, however
  // since the widget is not in focus, it does not block
  provider_->ObserveKeyEvents(
      kDeviceId, fake_observer->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(OpenAndCloseLauncher());
  EXPECT_FALSE(ModifierRewritesAreSuppressed());
  EXPECT_TRUE(InputDataProvider::ShouldCloseDialogOnEscape());
}

TEST_F(InputDataProviderTest, ShortcutBlockingObeysFocusSwitching) {
  const std::string kDevicePath("/dev/input/event6");
  const uint32_t kDeviceId = 6u;

  std::unique_ptr<FakeKeyboardObserver> fake_observer =
      std::make_unique<FakeKeyboardObserver>();

  provider_->attached_widget_->Show();

  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath(kDevicePath));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(OpenAndCloseLauncher());
  EXPECT_FALSE(ModifierRewritesAreSuppressed());
  EXPECT_TRUE(InputDataProvider::ShouldCloseDialogOnEscape());

  // If widget is in focus, ObserveKeyEvents should block shortcuts
  provider_->ObserveKeyEvents(
      kDeviceId, fake_observer->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(OpenAndCloseLauncher());
  EXPECT_TRUE(ModifierRewritesAreSuppressed());
  EXPECT_FALSE(InputDataProvider::ShouldCloseDialogOnEscape());

  // Hide widget and check that we can use shortcuts
  provider_->attached_widget_->Hide();
  EXPECT_TRUE(OpenAndCloseLauncher());
  EXPECT_FALSE(ModifierRewritesAreSuppressed());
  EXPECT_TRUE(InputDataProvider::ShouldCloseDialogOnEscape());

  // Show widget and check that shortcuts are blocked
  provider_->attached_widget_->Show();
  EXPECT_FALSE(OpenAndCloseLauncher());
  EXPECT_TRUE(ModifierRewritesAreSuppressed());
  EXPECT_FALSE(InputDataProvider::ShouldCloseDialogOnEscape());
}

TEST_F(InputDataProviderTest, ShortcutBlockingObeysLastObserverDisconnect) {
  const std::string kDevicePath("/dev/input/event6");
  const uint32_t kDeviceId = 6u;

  std::unique_ptr<FakeKeyboardObserver> fake_observer1 =
      std::make_unique<FakeKeyboardObserver>();
  std::unique_ptr<FakeKeyboardObserver> fake_observer2 =
      std::make_unique<FakeKeyboardObserver>();

  provider_->attached_widget_->Show();
  provider_->attached_widget_->Activate();

  // Shortcuts are still available after device is added
  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath(kDevicePath));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(OpenAndCloseLauncher());
  EXPECT_FALSE(ModifierRewritesAreSuppressed());
  EXPECT_TRUE(InputDataProvider::ShouldCloseDialogOnEscape());

  // If widget is in focus, ObserveKeyEvents should block shortcuts
  provider_->ObserveKeyEvents(
      kDeviceId, fake_observer1->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(OpenAndCloseLauncher());
  EXPECT_TRUE(ModifierRewritesAreSuppressed());
  EXPECT_FALSE(InputDataProvider::ShouldCloseDialogOnEscape());

  // When second observer is added, shortcuts should still be blocked
  provider_->ObserveKeyEvents(
      kDeviceId, fake_observer2->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(OpenAndCloseLauncher());
  EXPECT_TRUE(ModifierRewritesAreSuppressed());
  EXPECT_FALSE(InputDataProvider::ShouldCloseDialogOnEscape());

  // When first observer is destroyed, shortcuts should still be blocked
  fake_observer1.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(OpenAndCloseLauncher());
  EXPECT_TRUE(ModifierRewritesAreSuppressed());
  EXPECT_FALSE(InputDataProvider::ShouldCloseDialogOnEscape());

  // After second observer is destroyed, shortcuts should be unblocked
  fake_observer2.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(OpenAndCloseLauncher());
  EXPECT_FALSE(ModifierRewritesAreSuppressed());
  EXPECT_TRUE(InputDataProvider::ShouldCloseDialogOnEscape());
}

// Test overlapping lifetimes of separate observers of one device.
TEST_F(InputDataProviderTest, KeyObservationOverlappingeObserversOfDevice) {
  std::unique_ptr<FakeKeyboardObserver> fake_observer1 =
      std::make_unique<FakeKeyboardObserver>();

  provider_->attached_widget_->Show();

  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  provider_->ObserveKeyEvents(
      6u, fake_observer1->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, fake_observer1->events_.size());
  EXPECT_EQ(1u, provider_->watchers_->size());
  EXPECT_TRUE((*provider_->watchers_)[6]);

  std::unique_ptr<FakeKeyboardObserver> fake_observer2 =
      std::make_unique<FakeKeyboardObserver>();

  provider_->ObserveKeyEvents(
      6u, fake_observer2->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, fake_observer1->events_.size());
  EXPECT_EQ(0u, fake_observer2->events_.size());
  EXPECT_TRUE((*provider_->watchers_)[6]);

  fake_observer1.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, fake_observer2->events_.size());
  ASSERT_TRUE((*provider_->watchers_)[6]);

  // And send an event through to check functionality.
  (*provider_->watchers_)[6]->PostKeyEvent(true, kKeyA.key_code,
                                           kKeyA.at_scan_code);
  base::RunLoop().RunUntilIdle();

  // Ensure an event comes through properly after all that.
  EXPECT_EQ(1u, fake_observer2->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kEvent, fake_observer2->events_[0].first);
  ASSERT_TRUE(fake_observer2->events_[0].second);
  EXPECT_EQ(*fake_observer2->events_[0].second,
            mojom::KeyEvent(/*id=*/6u, /*type=*/mojom::KeyEventType::kPress,
                            /*key_code=*/kKeyA.key_code,
                            /*scan_code=*/kKeyA.at_scan_code,
                            /*top_row_position=*/-1));

  fake_observer2.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, provider_->watchers_->count(6));
}

// Double-check security model and ensure that multiple instances
// do not interfere with each other, and that key observations obey
// individual window focus combined with multiple instances.
TEST_F(InputDataProviderTest, KeyObservationMultipleProviders) {
  // Create a second InputDataProvider, with a separate window/widget,
  // as would happen if multiple instances of the SWA were created.
  watchers_t provider2_watchers;
  auto provider2_widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  std::unique_ptr<TestInputDataProvider> provider2_ =
      std::make_unique<TestInputDataProvider>(provider2_widget.get(),
                                              provider2_watchers,
                                              &event_rewriter_delegate_);
  auto& provider1_ = provider_;

  std::unique_ptr<FakeKeyboardObserver> fake_observer1 =
      std::make_unique<FakeKeyboardObserver>();
  std::unique_ptr<FakeKeyboardObserver> fake_observer2 =
      std::make_unique<FakeKeyboardObserver>();

  // Show and activate first window.
  provider1_->attached_widget_->Show();
  // Show and activate second window; this will deactivate the first window.
  provider2_->attached_widget_->Show();

  EXPECT_FALSE(provider1_->attached_widget_->IsActive());
  EXPECT_TRUE(provider2_->attached_widget_->IsActive());

  // Construct a keyboard.
  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event6"));
  provider1_->OnDeviceEvent(event0);
  provider2_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(provider1_->watchers_->empty());
  EXPECT_TRUE(provider2_->watchers_->empty());

  // Connected observer 1 to provider 1.
  provider1_->ObserveKeyEvents(
      6u, fake_observer1->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(provider1_->watchers_->empty());
  EXPECT_TRUE(provider2_->watchers_->empty());
  EXPECT_EQ(1u, fake_observer1->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kPause, fake_observer1->events_[0].first);
  EXPECT_EQ(1u, provider1_->watchers_->count(6));

  EXPECT_EQ(0u, fake_observer2->events_.size());
  EXPECT_EQ(0u, provider2_->watchers_->count(6));

  // Connected observer 2 to provider 2.

  provider2_->ObserveKeyEvents(
      6u, fake_observer2->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(provider1_->watchers_->empty());
  EXPECT_FALSE(provider2_->watchers_->empty());
  EXPECT_EQ(1u, fake_observer1->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kPause, fake_observer1->events_[0].first);
  EXPECT_EQ(1u, provider1_->watchers_->size());
  EXPECT_EQ(1u, provider1_->watchers_->size());
  ASSERT_TRUE((*provider1_->watchers_)[6]);
  ASSERT_TRUE((*provider2_->watchers_)[6]);

  EXPECT_EQ(0u, fake_observer2->events_.size());
  EXPECT_EQ(1u, provider2_->watchers_->size());
  EXPECT_TRUE((*provider2_->watchers_)[6]);
  // Providers should have distinct Watcher instances.
  EXPECT_NE((*provider1_->watchers_)[6], (*provider2_->watchers_)[6]);

  // Reset event logs for next round.
  fake_observer1->events_.clear();
  fake_observer2->events_.clear();

  // Post two separate key events.
  (*provider1_->watchers_)[6]->PostKeyEvent(true, kKeyA.key_code,
                                            kKeyA.at_scan_code);
  (*provider2_->watchers_)[6]->PostKeyEvent(true, kKeyB.key_code,
                                            kKeyB.at_scan_code);
  base::RunLoop().RunUntilIdle();

  // Ensure the events came through to expected targets.
  EXPECT_EQ(0u, fake_observer1->events_.size());

  EXPECT_EQ(1u, fake_observer2->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kEvent, fake_observer2->events_[0].first);
  ASSERT_TRUE(fake_observer2->events_[0].second);
  EXPECT_EQ(*fake_observer2->events_[0].second,
            mojom::KeyEvent(/*id=*/6u, /*type=*/mojom::KeyEventType::kPress,
                            /*key_code=*/kKeyB.key_code,
                            /*scan_code=*/kKeyB.at_scan_code,
                            /*top_row_position=*/-1));

  // Reset event logs for next round.
  fake_observer1->events_.clear();
  fake_observer2->events_.clear();

  // Switch active window.
  provider1_->attached_widget_->Activate();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(provider1_->attached_widget_->IsActive());
  EXPECT_TRUE(provider1_->attached_widget_->IsVisible());
  EXPECT_FALSE(provider2_->attached_widget_->IsActive());

  (*provider1_->watchers_)[6]->PostKeyEvent(true, kKeyA.key_code,
                                            kKeyA.at_scan_code);
  (*provider2_->watchers_)[6]->PostKeyEvent(true, kKeyB.key_code,
                                            kKeyB.at_scan_code);
  base::RunLoop().RunUntilIdle();

  // Ensure the events came through to expected targets.

  EXPECT_EQ(2u, fake_observer1->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kResume, fake_observer1->events_[0].first);
  EXPECT_FALSE(fake_observer1->events_[0].second);
  EXPECT_EQ(FakeKeyboardObserver::kEvent, fake_observer1->events_[1].first);
  ASSERT_TRUE(fake_observer1->events_[1].second);
  EXPECT_EQ(*fake_observer1->events_[1].second,
            mojom::KeyEvent(/*id=*/6u, /*type=*/mojom::KeyEventType::kPress,
                            /*key_code=*/kKeyA.key_code,
                            /*scan_code=*/kKeyA.at_scan_code,
                            /*top_row_position=*/-1));

  EXPECT_EQ(1u, fake_observer2->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kPause, fake_observer2->events_[0].first);
  EXPECT_FALSE(fake_observer2->events_[0].second);

  // Reset event logs for next round.
  fake_observer1->events_.clear();
  fake_observer2->events_.clear();

  // Activate a new widget, ensuring neither previous window is active.
  auto widget3 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget3->Activate();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(provider1_->attached_widget_->IsActive());
  EXPECT_FALSE(provider2_->attached_widget_->IsActive());

  // Event should show key paused from previously active window.
  EXPECT_EQ(1u, fake_observer1->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kPause, fake_observer1->events_[0].first);
  EXPECT_FALSE(fake_observer1->events_[0].second);

  // Reset event logs for next round.
  fake_observer1->events_.clear();
  fake_observer2->events_.clear();

  // Deliver keys to both.
  (*provider1_->watchers_)[6]->PostKeyEvent(true, kKeyA.key_code,
                                            kKeyA.at_scan_code);
  (*provider2_->watchers_)[6]->PostKeyEvent(true, kKeyB.key_code,
                                            kKeyB.at_scan_code);
  base::RunLoop().RunUntilIdle();

  // Neither window is visible and active, no events should be received.
  EXPECT_FALSE(provider1_->attached_widget_->IsVisible() &&
               provider1_->attached_widget_->IsActive());
  EXPECT_FALSE(provider2_->attached_widget_->IsVisible() &&
               provider2_->attached_widget_->IsActive());
  EXPECT_EQ(0u, fake_observer1->events_.size());
  EXPECT_EQ(0u, fake_observer2->events_.size());
}

TEST_F(InputDataProviderTest, KeyObservationTopRowBasic) {
  // Test with Eve keyboard: [Escape, Back, ..., Louder, Menu]
  std::unique_ptr<FakeKeyboardObserver> fake_observer =
      std::make_unique<FakeKeyboardObserver>();

  // Widget must be active and visible.
  provider_->attached_widget_->Show();
  provider_->attached_widget_->Activate();

  // Construct a keyboard.
  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  // Attach a key observer.
  provider_->ObserveKeyEvents(
      6u, fake_observer->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE((*provider_->watchers_)[6]);

  EXPECT_KEY_EVENTS(fake_observer.get(), 6u,
                    {{kKeyEsc, -1},
                     {kKeyF1, 0},
                     {kKeyF10, 9},
                     {kKeyMenu, -1},
                     {kKeyDelete, -1}});
}

TEST_F(InputDataProviderTest, KeyObservationTopRowUnknownAction) {
  // Test for Vivaldi descriptor having an unrecognized scan-code;
  // most likely due to external keyboard being newer than OS image.

  std::unique_ptr<FakeKeyboardObserver> fake_observer =
      std::make_unique<FakeKeyboardObserver>();

  // Widget must be active and visible.
  provider_->attached_widget_->Show();
  provider_->attached_widget_->Activate();

  std::vector<mojom::TopRowKey> modified_top_row_keys =
      std::vector(std::begin(kInternalJinlonTopRowKeys),
                  std::end(kInternalJinlonTopRowKeys));
  modified_top_row_keys[kUnknownScancodeIndex] = mojom::TopRowKey::kUnknown;

  // Construct a keyboard.
  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event11"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());

  const auto& keyboards = future.Get<0>();

  ASSERT_EQ(1ul, keyboards.size());
  const mojom::KeyboardInfoPtr& keyboard = keyboards[0];
  EXPECT_EQ(11u, keyboard->id);
  EXPECT_EQ(modified_top_row_keys, keyboard->top_row_keys);

  // Attach a key observer.
  provider_->ObserveKeyEvents(
      11u, fake_observer->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE((*provider_->watchers_)[11]);

  EXPECT_KEY_EVENTS(fake_observer.get(), 11u,
                    {{kKeyEsc, -1},
                     {kKeyActionBack, 0},
                     {kKeyF1, 0},
                     {kKeyActionRefresh, 1},
                     {kKeyActionFullscreen, 2},
                     {kKeyActionOverview, 3},
                     {kKeyActionScreenshot, 4},
                     {kKeyActionScreenBrightnessDown, 5},
                     {kKeyActionScreenBrightnessUp, 6},
                     {{0, kUnknownScancode, 0}, kUnknownScancodeIndex},
                     {kKeyF8, 7},
                     {kKeyActionKeyboardBrightnessDown, 8},
                     {kKeyActionKeyboardBrightnessUp, 9},
                     {kKeyActionKeyboardVolumeMute, 10},
                     {kKeyF10, 9},
                     {kKeyActionKeyboardVolumeDown, 11},
                     {kKeyActionKeyboardVolumeUp, 12},
                     {kKeySleep, -1}});
}

// TODO(b/208729519): Not available until we can test Drallion keyboards.
#if 0
TEST_F(InputDataProviderTest, KeyObservationTopRowDrallion) {
  // Test with Drallion keyboard:
  //  [Escape, Back, ..., Louder, F10, F11, F12, Mirror, Delete]
  // ...

  // Construct a keyboard
  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event10"));
  // ...
  struct {
    KeyDefinition key;
    int position;
  } keys[] = {
      {kKeyA, -1},
      {kKeyB, -1},
      {kKeyEsc, -1},
      {kKeyF1, 0},
      {kKeyF10, 9},
      {kKeyF11, 10},
      {kKeyF12, 11},
      {kKeySwitchVideoMode, 12},
      {kKeyDelete, 13},
  };

  for (size_t i = 0; i < std::size(keys); i++) {
    auto item = keys[i];
    provider_->watchers_[10]->PostKeyEvent(true, item.key.key_code,
         item.key.at_scan_code);
  }
  base::RunLoop().RunUntilIdle();

  // ...
}
#endif  // 0

TEST_F(InputDataProviderTest, KeyObservationTopRowExternalUSB) {
  std::unique_ptr<FakeKeyboardObserver> fake_observer =
      std::make_unique<FakeKeyboardObserver>();

  // Widget must be active and visible.
  provider_->attached_widget_->Show();
  provider_->attached_widget_->Activate();

  // Construct a keyboard.
  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event9"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  // Attach a key observer.
  provider_->ObserveKeyEvents(
      9u, fake_observer->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE((*provider_->watchers_)[9]);

  // Test with generic external keyboard.
  EXPECT_KEY_EVENTS(fake_observer.get(), 9u,
                    {{kKeyA, -1},
                     {kKeyB, -1},
                     {kKeyMenu, -1},
                     {kKeyDelete, -1},
                     {kKeyEsc, -1},
                     {kKeyF1, 0},
                     {kKeyF10, 9},
                     {kKeyF11, 10},
                     {kKeyF12, 11}});
}

TEST_F(InputDataProviderTest, KeyboardInputLog) {
  base::ScopedTempDir temp_dir;
  base::FilePath log_path;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  log_path = temp_dir.GetPath();
  const auto full_log_path = log_path.AppendASCII("keyboard_input.log");
  DiagnosticsLogController::Get()->SetKeyboardInputLogForTesting(
      std::make_unique<KeyboardInputLog>(log_path));
  std::unique_ptr<FakeKeyboardObserver> fake_observer =
      std::make_unique<FakeKeyboardObserver>();

  // Widget must be active and visible.
  provider_->attached_widget_->Show();
  provider_->attached_widget_->Activate();

  // Construct a keyboard.
  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  // Attach a key observer.
  provider_->ObserveKeyEvents(
      6u, fake_observer->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE((*provider_->watchers_)[6]);

  // Test a key event.
  EXPECT_KEY_EVENTS(fake_observer.get(), 6u, {{kKeyA, -1}});

  // Disconnect keyboard while it is being observed.
  ui::DeviceEvent remove_kbd_event(ui::DeviceEvent::DeviceType::INPUT,
                                   ui::DeviceEvent::ActionType::REMOVE,
                                   base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(remove_kbd_event);
  base::RunLoop().RunUntilIdle();

  // Watcher should have been shut down, and receiver disconnected.
  EXPECT_FALSE((*provider_->watchers_)[6]);
  task_environment()->RunUntilIdle();
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(full_log_path, &contents));

  std::vector<std::string> lines = GetLogLines(contents);
  ASSERT_EQ(2u, lines.size());

  // First line is of form:
  // [TimeStamp] - Key press test - AT Translated Set 2 keyboard
  std::vector<std::string> first_line_contents =
      GetLogLineContents(lines[0], "-");
  ASSERT_EQ(3u, first_line_contents.size());
  EXPECT_EQ("Key press test", first_line_contents[1]);
  EXPECT_EQ("AT Translated Set 2 keyboard", first_line_contents[2]);

  EXPECT_EQ("Key code: 30, Scan code: 30", lines[1]);
}

TEST_F(InputDataProviderTest, KeyboardTesterRoutineDurationMetric) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<FakeKeyboardObserver> fake_observer =
      std::make_unique<FakeKeyboardObserver>();

  // Widget must be active and visible.
  provider_->attached_widget_->Show();
  provider_->attached_widget_->Activate();

  // Construct a keyboard.
  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  // Attach a key observer.
  provider_->ObserveKeyEvents(
      /*id=*/6u, fake_observer->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  task_environment()->FastForwardBy(
      base::Seconds(kKeyboardTesterMetricTimeDelay));
  ASSERT_TRUE((*provider_->watchers_)[6]);

  // Test a key event.
  EXPECT_KEY_EVENTS(fake_observer.get(), /*id=*/6u, {{kKeyA, -1}});

  // Disconnect keyboard while it is being observed.
  ui::DeviceEvent remove_kbd_event(ui::DeviceEvent::DeviceType::INPUT,
                                   ui::DeviceEvent::ActionType::REMOVE,
                                   base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(remove_kbd_event);
  base::RunLoop().RunUntilIdle();

  // Watcher should have been shut down, and receiver disconnected.
  EXPECT_FALSE((*provider_->watchers_)[6]);
  task_environment()->RunUntilIdle();

  histogram_tester.ExpectUniqueTimeSample(
      "ChromeOS.DiagnosticsUi.KeyboardTesterRoutineDuration",
      base::Seconds(kKeyboardTesterMetricTimeDelay),
      /*expected_bucket_count=*/1);
}

TEST_F(InputDataProviderTest,
       KeyboardTesterRoutineDurationMetricOnDestruction) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<FakeKeyboardObserver> fake_observer =
      std::make_unique<FakeKeyboardObserver>();

  // Widget must be active and visible.
  provider_->attached_widget_->Show();
  provider_->attached_widget_->Activate();

  // Construct a keyboard.
  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  // Attach a key observer.
  provider_->ObserveKeyEvents(
      /*id=*/6u, fake_observer->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  task_environment()->FastForwardBy(
      base::Seconds(kKeyboardTesterMetricTimeDelay));
  ASSERT_TRUE((*provider_->watchers_)[6]);

  // Test a key event.
  EXPECT_KEY_EVENTS(fake_observer.get(), /*id=*/6u, {{kKeyA, -1}});

  // Manually destroy the provider.
  provider_.reset();

  histogram_tester.ExpectUniqueTimeSample(
      "ChromeOS.DiagnosticsUi.KeyboardTesterRoutineDuration",
      base::Seconds(kKeyboardTesterMetricTimeDelay),
      /*expected_bucket_count=*/1);
}

// TODO(b/211780758): Test all Fx scancodes using
// ui/events/keycodes/dom/dom_code_data.inc as source of truth.

// Test the behavior when the tablet mode status has changed. The tablet mode
// is initialized as "not-in-tablet-mode".
TEST_F(InputDataProviderTest, TabletModeObservation) {
  FakeTabletModeObserver fake_observer;
  base::test::TestFuture<bool> future;

  // Attach a tablet mode observer.
  provider_->ObserveTabletMode(
      fake_observer.receiver.BindNewPipeAndPassRemote(), future.GetCallback());

  // Default initial state is "not-in-tablet-mode".
  ASSERT_FALSE(future.Get<0>());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(fake_observer.is_tablet_mode());
  EXPECT_EQ(1u, fake_observer.num_tablet_mode_change_calls());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(fake_observer.is_tablet_mode());
  EXPECT_EQ(2u, fake_observer.num_tablet_mode_change_calls());
}

// Test the behavior when the tablet mode status has changed. The tablet mode
// is initialized as "in-tablet-mode".
TEST_F(InputDataProviderTest, TabletModeObservationInitAsTabletMode) {
  FakeTabletModeObserver fake_observer;
  base::test::TestFuture<bool> future;

  // Set initial state as tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  // Attach a tablet mode observer.
  provider_->ObserveTabletMode(
      fake_observer.receiver.BindNewPipeAndPassRemote(), future.GetCallback());

  // Initial state is set to "in-tablet-mode".
  ASSERT_TRUE(future.Get<0>());
}

// Test the behavior when the lid state has changed. The lid state is
// initialized as open.
TEST_F(InputDataProviderTest, LidStateObservation) {
  // Need to run loop here to give constructor time to receive response from
  // FakePowerManagerClient.
  base::RunLoop().RunUntilIdle();

  FakeLidStateObserver fake_observer;
  base::test::TestFuture<bool> future;

  power_manager_client()->SetLidState(
      chromeos::PowerManagerClient::LidState::OPEN, /*timestamp=*/{});

  // Attach a lid state observer.
  provider_->ObserveLidState(fake_observer.receiver.BindNewPipeAndPassRemote(),
                             future.GetCallback());
  base::RunLoop().RunUntilIdle();

  // Default state is that lid is open.
  ASSERT_TRUE(future.Get<0>());

  power_manager_client()->SetLidState(
      chromeos::PowerManagerClient::LidState::CLOSED, /*timestamp=*/{});
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(fake_observer.is_lid_open());
  EXPECT_EQ(1u, fake_observer.num_lid_state_change_calls());

  power_manager_client()->SetLidState(
      chromeos::PowerManagerClient::LidState::OPEN, /*timestamp=*/{});
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(fake_observer.is_lid_open());
  EXPECT_EQ(2u, fake_observer.num_lid_state_change_calls());
}

// Test the behavior when the lid state status has changed. The lid state is
// initialized as closed.
TEST_F(InputDataProviderTest, LidStateObservationInitAsClosed) {
  // Need to run loop here to give constructor time to receive response from
  // FakePowerManagerClient.
  base::RunLoop().RunUntilIdle();

  FakeLidStateObserver fake_observer;
  base::test::TestFuture<bool> future;

  power_manager_client()->SetLidState(
      chromeos::PowerManagerClient::LidState::CLOSED, /*timestamp=*/{});

  // Attach a tablet mode observer.
  provider_->ObserveLidState(fake_observer.receiver.BindNewPipeAndPassRemote(),
                             future.GetCallback());
  base::RunLoop().RunUntilIdle();

  // Default state is that lid is closed.
  ASSERT_FALSE(future.Get<0>());
}

// Test the behavior when the lid state status has changed. The lid state is
// initialized as closed.
TEST_F(InputDataProviderTest, LidStateObservationInitAsUnsupported) {
  // Need to run loop here to give constructor time to receive response from
  // FakePowerManagerClient.
  base::RunLoop().RunUntilIdle();

  FakeLidStateObserver fake_observer;
  base::test::TestFuture<bool> future;

  power_manager_client()->SetLidState(
      chromeos::PowerManagerClient::LidState::NOT_PRESENT, /*timestamp=*/{});

  // Attach a tablet mode observer.
  provider_->ObserveLidState(fake_observer.receiver.BindNewPipeAndPassRemote(),
                             future.GetCallback());
  base::RunLoop().RunUntilIdle();

  // Default state is that lid is unsupported, which should act as though lid is
  // open.
  ASSERT_TRUE(future.Get<0>());
}

// Test the behavior when the initial internal display power state is on.
TEST_F(InputDataProviderTest, InternalDisplayPowerStateAsDefault) {
  FakeInternalDisplayPowerStateObserver fake_observer;

  // Attach a internal display power state observer.
  provider_->ObserveInternalDisplayPowerState(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  ASSERT_TRUE(provider_->is_internal_display_on());
}

// Test the behavior when the initial internal display power state is off.
TEST_F(InputDataProviderTest, InternalDisplayPowerStateAsOff) {
  FakeInternalDisplayPowerStateObserver fake_observer;

  // Set initial display state as internal off and external on.
  auto* displayConfigurator = Shell::Get()->display_configurator();
  displayConfigurator->reset_requested_power_state_for_test();
  displayConfigurator->SetInitialDisplayPower(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON);

  // Attach a internal display power state observer.
  provider_->ObserveInternalDisplayPowerState(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  ASSERT_FALSE(provider_->is_internal_display_on());
}

// Test the behavior when the internal display power state has changed.
TEST_F(InputDataProviderTest, InternalDisplayPowerStateObserver) {
  FakeInternalDisplayPowerStateObserver fake_observer;

  // Attach a internal display power state observer.
  provider_->ObserveInternalDisplayPowerState(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  provider_->OnPowerStateChanged(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(fake_observer.is_display_on());
  EXPECT_EQ(1u, fake_observer.num_display_state_change_calls());

  provider_->OnPowerStateChanged(
      chromeos::DISPLAY_POWER_INTERNAL_ON_EXTERNAL_OFF);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(fake_observer.is_display_on());
  EXPECT_EQ(2u, fake_observer.num_display_state_change_calls());
}

// Test the behavior if the app is not already in the testing touchscreen, the
// app should be moved to the targeting touchscreen.
TEST_F(InputDataProviderTest, MoveAppToTestingScreen) {
  // Construct two touchscreens. "/dev/input/event2" and "/dev/input/event14"
  // map to touchscreens.
  ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event2"));
  ui::DeviceEvent event1(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event14"));
  provider_->OnDeviceEvent(event0);
  provider_->OnDeviceEvent(event1);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      getConnectedDevicesFuture;
  provider_->GetConnectedDevices(getConnectedDevicesFuture.GetCallback());

  const auto& keyboards = getConnectedDevicesFuture.Get<0>();
  const auto& touch_devices = getConnectedDevicesFuture.Get<1>();

  ASSERT_EQ(0ul, keyboards.size());
  ASSERT_EQ(2ul, touch_devices.size());

  // Set up three fake displays.
  UpdateDisplay("500x400, 600x400, 800x600");
  display::Screen* screen = display::Screen::GetScreen();
  const int64_t primary_display_id = screen->GetAllDisplays()[0].id();
  const int64_t secondary_display_id = screen->GetAllDisplays()[1].id();
  const int64_t third_display_id = screen->GetAllDisplays()[2].id();

  // Initialize two touchscreens in DeviceDataManager.
  std::vector<ui::TouchscreenDevice> touchscreen_devices;
  touchscreen_devices.push_back(ui::TouchscreenDevice(
      kDeviceId1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      touch_devices[0]->name, /*size=*/gfx::Size(600, 400),
      /*touch_points=*/0));
  touchscreen_devices.push_back(ui::TouchscreenDevice(
      kDeviceId2, ui::InputDeviceType::INPUT_DEVICE_USB, touch_devices[1]->name,
      /*size=*/gfx::Size(800, 600),
      /*touch_points=*/0));
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(touchscreen_devices);

  // Associate the touchscreens in DeviceDataManager to the fake displays.
  std::vector<ui::TouchDeviceTransform> touch_device_transforms(2);
  touch_device_transforms[0].display_id = secondary_display_id;
  touch_device_transforms[0].device_id = kDeviceId1;
  touch_device_transforms[1].display_id = third_display_id;
  touch_device_transforms[1].device_id = kDeviceId2;
  ui::DeviceDataManager::GetInstance()->ConfigureTouchDevices(
      touch_device_transforms);

  // Before move: make sure the app is currently in the primary display.
  aura::Window* window = widget_->GetNativeWindow();
  ASSERT_EQ(primary_display_id, screen->GetDisplayNearestWindow(window).id());

  // Call MoveAppToTestingScreen function with the touchscreen evdev id 2,
  // which maps to the secondary display.
  provider_->MoveAppToTestingScreen(/*evdev_id=*/2);

  // Confirm the app has been moved to the secondary display.
  ASSERT_EQ(secondary_display_id, screen->GetDisplayNearestWindow(window).id());

  // Call MoveAppToTestingScreen function with the touchscreen evdev id 14,
  // which maps to the third display.
  provider_->MoveAppToTestingScreen(/*evdev_id=*/14);

  // Confirm the app has been moved to the third display.
  ASSERT_EQ(third_display_id, screen->GetDisplayNearestWindow(window).id());
}

// Test the app can be moved back to original screen.
TEST_F(InputDataProviderTest, MoveAppBackToPreviousScreen) {
  // Construct a touchscreen. "/dev/input/event2" maps to a touchscreen.
  ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event2"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      getConnectedDevicesFuture;
  provider_->GetConnectedDevices(getConnectedDevicesFuture.GetCallback());

  const auto& keyboards = getConnectedDevicesFuture.Get<0>();
  const auto& touch_devices = getConnectedDevicesFuture.Get<1>();

  ASSERT_EQ(0ul, keyboards.size());
  ASSERT_EQ(1ul, touch_devices.size());

  // Set up two fake displays.
  UpdateDisplay("500x400, 600x400");
  display::Screen* screen = display::Screen::GetScreen();
  const int64_t primary_display_id = screen->GetAllDisplays()[0].id();
  const int64_t secondary_display_id = screen->GetAllDisplays()[1].id();

  // Initialize one touchscreen in DeviceDataManager.
  std::vector<ui::TouchscreenDevice> touchscreen_devices;
  touchscreen_devices.push_back(ui::TouchscreenDevice(
      kDeviceId1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      touch_devices[0]->name, /*size=*/gfx::Size(600, 400),
      /*touch_points=*/0));
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(touchscreen_devices);

  // Associate the touchscreen in DeviceDataManager to the fake display.
  std::vector<ui::TouchDeviceTransform> touch_device_transforms(1);
  touch_device_transforms[0].display_id = secondary_display_id;
  touch_device_transforms[0].device_id = kDeviceId1;
  ui::DeviceDataManager::GetInstance()->ConfigureTouchDevices(
      touch_device_transforms);

  // Before move: make sure the app is currently in the primary display.
  aura::Window* window = widget_->GetNativeWindow();
  ASSERT_EQ(primary_display_id, screen->GetDisplayNearestWindow(window).id());

  // Call MoveAppToTestingScreen function with the touchscreen evdev id 2,
  // which maps to the secondary display.
  provider_->MoveAppToTestingScreen(/*evdev_id=*/2);

  // Confirm the app has been moved to the secondary display.
  ASSERT_EQ(secondary_display_id, screen->GetDisplayNearestWindow(window).id());

  // Call MoveAppBackToPreviousScreen to move the app back to original
  // display.
  provider_->MoveAppBackToPreviousScreen();

  // Confirm the app has been moved back to the original display.
  ASSERT_EQ(primary_display_id, screen->GetDisplayNearestWindow(window).id());
}

TEST_F(InputDataProviderTest, SetA11yTouchPassthrough) {
  aura::Window* window = widget_->GetNativeWindow();

  // The value is false in default.
  ASSERT_FALSE(window->GetProperty(
      aura::client::kAccessibilityTouchExplorationPassThrough));

  provider_->SetA11yTouchPassthrough(/*enabled=*/true);

  ASSERT_TRUE(window->GetProperty(
      aura::client::kAccessibilityTouchExplorationPassThrough));

  provider_->SetA11yTouchPassthrough(/*enabled=*/false);

  ASSERT_FALSE(window->GetProperty(
      aura::client::kAccessibilityTouchExplorationPassThrough));
}

}  // namespace diagnostics
}  // namespace ash
