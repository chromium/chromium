// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_INPUT_DATA_PROVIDER_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_INPUT_DATA_PROVIDER_H_

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/system/diagnostics/mojom/input.mojom.h"
#include "ash/webui/diagnostics_ui/backend/input/event_watcher_factory.h"
#include "ash/webui/diagnostics_ui/backend/input/healthd_event_reporter.h"
#include "ash/webui/diagnostics_ui/backend/input/input_data_event_watcher.h"
#include "ash/webui/diagnostics_ui/backend/input/input_data_provider_keyboard.h"
#include "ash/webui/diagnostics_ui/backend/input/input_data_provider_touch.h"
#include "ash/webui/diagnostics_ui/backend/input/input_device_information.h"
#include "ash/webui/diagnostics_ui/backend/input/keyboard_input_data_event_watcher.h"
#include "ash/webui/diagnostics_ui/mojom/input_data_provider.mojom.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/ash/event_rewriter_ash.h"
#include "ui/events/ozone/device/device_event.h"
#include "ui/events/ozone/device/device_event_observer.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ash::diagnostics {

// Provides information about input devices connected to the system. Implemented
// in the browser process, constructed within the Diagnostics_UI in the browser
// process, and eventually called by the Diagnostics SWA (a renderer process).
class InputDataProvider : public mojom::InputDataProvider,
                          public ui::DeviceEventObserver,
                          public KeyboardInputDataEventWatcher::Dispatcher,
                          public views::WidgetObserver,
                          public TabletModeObserver,
                          public display::DisplayConfigurator::Observer,
                          public chromeos::PowerManagerClient::Observer {
 public:
  explicit InputDataProvider(aura::Window* window);
  explicit InputDataProvider(
      aura::Window* window,
      std::unique_ptr<ui::DeviceManager> device_manager,
      std::unique_ptr<EventWatcherFactory> watcher_factory,
      AcceleratorControllerImpl* accelerator_controller,
      ui::EventRewriterAsh::Delegate* event_rewriter_delegate);
  InputDataProvider(const InputDataProvider&) = delete;
  InputDataProvider& operator=(const InputDataProvider&) = delete;
  ~InputDataProvider() override;

  static bool ShouldCloseDialogOnEscape() {
    return should_close_dialog_on_escape_;
  }

  void BindInterface(
      mojo::PendingReceiver<mojom::InputDataProvider> pending_receiver);

  static mojom::ConnectionType ConnectionTypeFromInputDeviceType(
      ui::InputDeviceType type);

  // KeyboardInputDataEventWatcher::Dispatcher:
  void SendInputKeyEvent(uint32_t id,
                         uint32_t key_code,
                         uint32_t scan_code,
                         bool down) override;

  // mojom::InputDataProvider:
  void GetConnectedDevices(GetConnectedDevicesCallback callback) override;

  void ObserveConnectedDevices(
      mojo::PendingRemote<mojom::ConnectedDevicesObserver> observer) override;

  void ObserveKeyEvents(
      uint32_t id,
      mojo::PendingRemote<mojom::KeyboardObserver> observer) override;

  void ObserveTabletMode(
      mojo::PendingRemote<mojom::TabletModeObserver> observer,
      ObserveTabletModeCallback callback) override;

  void ObserveLidState(mojo::PendingRemote<mojom::LidStateObserver> observer,
                       ObserveLidStateCallback callback) override;

  void ObserveInternalDisplayPowerState(
      mojo::PendingRemote<mojom::InternalDisplayPowerStateObserver> observer)
      override;

  void MoveAppToTestingScreen(uint32_t evdev_id) override;

  void MoveAppBackToPreviousScreen() override;

  void SetA11yTouchPassthrough(bool enabled) override;

  // ui::DeviceEventObserver:
  void OnDeviceEvent(const ui::DeviceEvent& event) override;

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // TabletModeObserver:
  void OnTabletModeEventsBlockingChanged() override;

  // chromeos::PowerManagerClient::Observer
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        base::TimeTicks time) override;
  void OnReceiveSwitchStates(
      std::optional<chromeos::PowerManagerClient::SwitchStates> switch_states);

  // display::DisplayConfigurator::Observer
  void OnPowerStateChanged(chromeos::DisplayPowerState power_state) override;

  // Get the value of is_internal_display_on_ for testing purpose.
  bool is_internal_display_on() { return is_internal_display_on_; }

 protected:
  base::SequenceBound<InputDeviceInfoHelper> info_helper_{
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})};

 private:
  void Initialize(aura::Window* window);

  void GetConnectedDevicesHelper(GetConnectedDevicesCallback callback);

  void ProcessDeviceInfo(std::unique_ptr<InputDeviceInformation> device_info);

  void AddTouchDevice(const InputDeviceInformation* device_info);
  void AddKeyboard(const InputDeviceInformation* device_info);

  bool may_send_events_ = false;

  // Review widget state to determine whether it is safe to send events.
  void UpdateMaySendEvents();
  // Pass on pause or resume events to observers if that state has changed.
  void UpdateEventObservers();

  void SendPauseEvents();
  void SendResumeEvents();

  void BlockShortcuts(bool should_block);

  InputDataProviderKeyboard keyboard_helper_;
  InputDataProviderTouch touch_helper_;

  // Handle destroyed KeyboardObservers.
  void OnObservedKeyboardInputDisconnect(uint32_t evdev_id,
                                         mojo::RemoteSetElementId observer_id);
  // Manage watchers that read an evdev and process events for observers.
  void ForwardKeyboardInput(uint32_t id);
  void UnforwardKeyboardInput(uint32_t id);

  const std::string GetKeyboardName(uint32_t id);

  // Denotes whether DiagnosticsDialog should be closed when escape is pressed.
  // Currently, this is only false when the keyboard tester is actively in use.
  static bool should_close_dialog_on_escape_;

  // Whether a tablet mode switch is present (which we use as a hint for the
  // top-right key glyph).
  bool has_tablet_mode_switch_ = false;

  // Whether the internal touchscreen is on, used to determine if the internal
  // display is testable or not.
  bool is_internal_display_on_ = true;

  // Whether the laptop lid is closed or open. On chromeboxes, this will always
  // be false.
  bool is_lid_open_ = false;

  // Id of the previous display the Diagnostics app was in, used to move the app
  // back to previous display when the touchscreen tester is closed.
  int64_t previous_display_id_ = display::kInvalidDisplayId;

  // Map by evdev ids to information blocks.
  base::flat_map<int, mojom::KeyboardInfoPtr> keyboards_;
  base::flat_map<int, std::unique_ptr<InputDataProviderKeyboard::AuxData>>
      keyboard_aux_data_;
  base::flat_map<int, mojom::TouchDeviceInfoPtr> touch_devices_;

  // Map by evdev ids to remote observers and event watchers.
  base::flat_map<int, std::unique_ptr<mojo::RemoteSet<mojom::KeyboardObserver>>>
      keyboard_observers_;
  base::flat_map<int, std::unique_ptr<InputDataEventWatcher>>
      keyboard_watchers_;

  // Timestamp of when keyboard tester is first opened. Undefined if the
  // keyboard tester is not open.
  base::Time keyboard_tester_start_timestamp_;

  bool logged_not_dispatching_key_events_ = false;
  raw_ptr<views::Widget> widget_ = nullptr;

  mojo::RemoteSet<mojom::ConnectedDevicesObserver> connected_devices_observers_;

  mojo::RemoteSet<mojom::TabletModeObserver> tablet_mode_observers_;

  mojo::RemoteSet<mojom::LidStateObserver> lid_state_observers_;

  mojo::Remote<mojom::InternalDisplayPowerStateObserver>
      internal_display_power_state_observer_;

  mojo::Receiver<mojom::InputDataProvider> receiver_{this};

  std::unique_ptr<ui::DeviceManager> device_manager_;

  std::unique_ptr<EventWatcherFactory> watcher_factory_;

  raw_ptr<AcceleratorControllerImpl> accelerator_controller_;
  raw_ptr<ui::EventRewriterAsh::Delegate> event_rewriter_delegate_;

  HealthdEventReporter healthd_event_reporter_;

  base::OnceCallback<void()> get_connected_devices_callback_;

  base::WeakPtrFactory<InputDataProvider> weak_factory_{this};
};

}  // namespace ash::diagnostics

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_INPUT_DATA_PROVIDER_H_
