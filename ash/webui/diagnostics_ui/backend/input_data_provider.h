// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_DATA_PROVIDER_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_DATA_PROVIDER_H_

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/webui/diagnostics_ui/backend/input_data_provider_keyboard.h"
#include "ash/webui/diagnostics_ui/backend/input_data_provider_touch.h"
#include "ash/webui/diagnostics_ui/mojom/input_data_provider.mojom.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/aura/window.h"
#include "ui/events/ozone/device/device_event.h"
#include "ui/events/ozone/device/device_event_observer.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ash::diagnostics {

// Interfaces for watching and dispatching relevant events from evdev to the
// input_data_provider.
class InputDataEventWatcher {
 public:
  class Dispatcher {
   public:
    virtual void SendInputKeyEvent(uint32_t id,
                                   uint32_t key_code,
                                   uint32_t scan_code,
                                   bool down) = 0;
  };

  class Factory {
   public:
    virtual ~Factory() = 0;

    virtual std::unique_ptr<InputDataEventWatcher> MakeWatcher(
        uint32_t evdev_id,
        base::WeakPtr<Dispatcher> dispatcher) = 0;
  };

  virtual ~InputDataEventWatcher() = 0;
};

// Wrapper for tracking several pieces of information about an evdev-backed
// device.
class InputDeviceInformation {
 public:
  InputDeviceInformation();
  InputDeviceInformation(const InputDeviceInformation& other) = delete;
  InputDeviceInformation& operator=(const InputDeviceInformation& other) =
      delete;
  ~InputDeviceInformation();

  int evdev_id;
  ui::EventDeviceInfo event_device_info;
  ui::InputDevice input_device;
  mojom::ConnectionType connection_type;
  base::FilePath path;

  // Keyboard-only fields:
  ui::EventRewriterChromeOS::DeviceType keyboard_type;
  ui::EventRewriterChromeOS::KeyboardTopRowLayout keyboard_top_row_layout;
  base::flat_map<uint32_t, ui::EventRewriterChromeOS::MutableKeyState>
      keyboard_scan_code_map;
};

// Class for running GetDeviceInfo in its own sequence, to allow it to block.
class InputDeviceInfoHelper {
 public:
  InputDeviceInfoHelper() {}
  virtual ~InputDeviceInfoHelper() {}

  virtual std::unique_ptr<InputDeviceInformation> GetDeviceInfo(
      int evdev_id,
      base::FilePath path);
};

// Provides information about input devices connected to the system. Implemented
// in the browser process, constructed within the Diagnostics_UI in the browser
// process, and eventually called by the Diagnostics SWA (a renderer process).
class InputDataProvider : public mojom::InputDataProvider,
                          public ui::DeviceEventObserver,
                          public InputDataEventWatcher::Dispatcher,
                          public views::WidgetObserver {
 public:
  explicit InputDataProvider(aura::Window* window);
  explicit InputDataProvider(
      aura::Window* window,
      std::unique_ptr<ui::DeviceManager> device_manager,
      std::unique_ptr<InputDataEventWatcher::Factory> watcher_factory);
  InputDataProvider(const InputDataProvider&) = delete;
  InputDataProvider& operator=(const InputDataProvider&) = delete;
  ~InputDataProvider() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::InputDataProvider> pending_receiver);

  static mojom::ConnectionType ConnectionTypeFromInputDeviceType(
      ui::InputDeviceType type);

  // InputDataEventWatcher::Dispatcher:
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

  // ui::DeviceEventObserver:
  void OnDeviceEvent(const ui::DeviceEvent& event) override;

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

 protected:
  base::SequenceBound<InputDeviceInfoHelper> info_helper_{
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})};

 private:
  void Initialize(aura::Window* window);

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

  // Whether a tablet mode switch is present (which we use as a hint for the
  // top-right key glyph).
  bool has_tablet_mode_switch_ = false;

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

  bool logged_not_dispatching_key_events_ = false;
  views::Widget* widget_ = nullptr;

  mojo::RemoteSet<mojom::ConnectedDevicesObserver> connected_devices_observers_;

  mojo::Receiver<mojom::InputDataProvider> receiver_{this};

  std::unique_ptr<ui::DeviceManager> device_manager_;

  std::unique_ptr<InputDataEventWatcher::Factory> watcher_factory_;

  base::WeakPtrFactory<InputDataProvider> weak_factory_{this};
};

}  // namespace ash::diagnostics

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_DATA_PROVIDER_H_
