// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/input_data_provider.h"

#include <fcntl.h>
#include <linux/input.h>
#include <vector>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "ui/events/devices/device_util_linux.h"
#include "ui/events/devices/input_device.h"

namespace ash {
namespace diagnostics {

namespace {

bool GetEventNodeId(base::FilePath path, int* id) {
  const std::string base_name_prefix = "event";

  std::string base_name = path.BaseName().value();
  if (!base::StartsWith(base_name, base_name_prefix))
    return false;
  base_name.erase(0, base_name_prefix.length());
  return base::StringToInt(base_name, id);
}

// Determine if this particular evdev provides touchpad or touchscreen input;
// we do not want stylus devices, which also claim to be touchscreens.
bool IsTouchInputDevice(InputDeviceInformation* device_info) {
  return (device_info->event_device_info.HasTouchpad() ||
          (device_info->event_device_info.HasTouchscreen() &&
           !device_info->event_device_info.HasStylus()));
}

const int kKeyReleaseValue = 0;

}  // namespace

// Class for dispatching relevant events from evdev to the input_data_provider.
// While it would be nice to re-use EventConverterEvdevImpl for this purpose,
// it has a lot of connections (ui::Cursor, full ui::DeviceEventDispatcherEvdev
// interface) that take more room to stub out rather than just implementing
// another evdev FdWatcher from scratch.
class InputDataEventWatcherImpl : public InputDataEventWatcher,
                                  base::MessagePumpForUI::FdWatcher {
 public:
  InputDataEventWatcherImpl(
      uint32_t id,
      base::WeakPtr<InputDataEventWatcher::Dispatcher> dispatcher);
  ~InputDataEventWatcherImpl() override;
  void ConvertKeyEvent(uint32_t key_code,
                       uint32_t key_state,
                       uint32_t scan_code);
  void ProcessEvent(const input_event& input);
  void Start();
  void Stop();

 protected:
  // base::MessagePumpForUI::FdWatcher:
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  // Device id
  const uint32_t id_;

  // Path to input device.
  const base::FilePath path_;

  // File descriptor to read.
  const int fd_;

  // Scoped auto-closer for FD.
  const base::ScopedFD input_device_fd_;

  // Whether we're polling for input on the device.
  bool watching_ = false;

  // EV_ information pending for SYN_REPORT to dispatch.
  uint32_t pending_scan_code_;
  uint32_t pending_key_code_;
  uint32_t pending_key_state_;

  base::WeakPtr<InputDataEventWatcher::Dispatcher> dispatcher_;

  // Controller for watching the input fd.
  base::MessagePumpForUI::FdWatchController controller_;
};

class InputDataEventWatcherFactoryImpl : public InputDataEventWatcher::Factory {
 public:
  InputDataEventWatcherFactoryImpl() = default;
  InputDataEventWatcherFactoryImpl(const InputDataEventWatcherFactoryImpl&) =
      delete;
  InputDataEventWatcherFactoryImpl& operator=(
      const InputDataEventWatcherFactoryImpl&) = delete;
  ~InputDataEventWatcherFactoryImpl() override = default;

  std::unique_ptr<InputDataEventWatcher> MakeWatcher(
      uint32_t id,
      base::WeakPtr<InputDataEventWatcher::Dispatcher> dispatcher) override {
    return std::make_unique<InputDataEventWatcherImpl>(id,
                                                       std::move(dispatcher));
  }
};

InputDataEventWatcher::~InputDataEventWatcher() = default;
InputDataEventWatcher::Factory::~Factory() = default;

InputDataEventWatcherImpl::InputDataEventWatcherImpl(
    uint32_t id,
    base::WeakPtr<InputDataEventWatcher::Dispatcher> dispatcher)
    : id_(id),
      path_(base::FilePath(base::StringPrintf("/dev/input/event%d", id_))),
      fd_(open(path_.value().c_str(), O_RDWR | O_NONBLOCK)),
      input_device_fd_(fd_),
      dispatcher_(dispatcher),
      controller_(FROM_HERE) {
  if (fd_ == -1) {
    PLOG(ERROR) << "Unable to open event device " << id_
                << ", not forwarding events for input diagnostics.";
    // Leave un-Started(), so we never enable the fd watcher.
    return;
  }

  Start();
}

InputDataEventWatcherImpl::~InputDataEventWatcherImpl() = default;

void InputDataEventWatcherImpl::Start() {
  base::CurrentUIThread::Get()->WatchFileDescriptor(
      fd_, true, base::MessagePumpForUI::WATCH_READ, &controller_, this);
  watching_ = true;
}

void InputDataEventWatcherImpl::Stop() {
  controller_.StopWatchingFileDescriptor();
  watching_ = false;
}

void InputDataEventWatcherImpl::OnFileCanReadWithoutBlocking(int fd) {
  while (true) {
    input_event input;
    ssize_t read_size = read(fd, &input, sizeof(input));
    if (read_size != sizeof(input)) {
      if (errno == EINTR || errno == EAGAIN)
        return;
      if (errno != ENODEV)
        PLOG(ERROR) << "error reading device " << path_.value();
      Stop();
      return;
    }

    ProcessEvent(input);
  }
}

void InputDataEventWatcherImpl::OnFileCanWriteWithoutBlocking(int fd) {}

// Once we have an entire keypress/release, dispatch it.
void InputDataEventWatcherImpl::ConvertKeyEvent(uint32_t key_code,
                                                uint32_t key_state,
                                                uint32_t scan_code) {
  bool down = key_state != kKeyReleaseValue;
  if (dispatcher_)
    dispatcher_->SendInputKeyEvent(id_, key_code, scan_code, down);
}

// Process evdev event structures directly from the kernel.
void InputDataEventWatcherImpl::ProcessEvent(const input_event& input) {
  // Accumulate relevant data about an event until a SYN_REPORT event releases
  // the full report. For more information, see kernel documentation for
  // input/event-codes.rst.
  switch (input.type) {
    case EV_MSC:
      if (input.code == MSC_SCAN)
        pending_scan_code_ = input.value;
      break;
    case EV_KEY:
      pending_key_code_ = input.code;
      pending_key_state_ = input.value;
      break;
    case EV_SYN:
      if (input.code == SYN_REPORT)
        ConvertKeyEvent(pending_key_code_, pending_key_state_,
                        pending_scan_code_);

      pending_key_code_ = 0;
      pending_key_state_ = 0;
      pending_scan_code_ = 0;
      break;
  }
}

// All blockings calls for identifying hardware need to go here: both
// EventDeviceInfo::Initialize and ui::GetInputPathInSys can block in
// base::MakeAbsoluteFilePath.
std::unique_ptr<InputDeviceInformation> InputDeviceInfoHelper::GetDeviceInfo(
    int id,
    base::FilePath path) {
  base::ScopedFD fd(open(path.value().c_str(), O_RDWR | O_NONBLOCK));
  if (fd.get() < 0) {
    PLOG(ERROR) << "Couldn't open device path " << path << ".";
    return nullptr;
  }

  auto info = std::make_unique<InputDeviceInformation>();

  if (!info->event_device_info.Initialize(fd.get(), path)) {
    LOG(ERROR) << "Failed to get device info for " << path << ".";
    return nullptr;
  }

  const base::FilePath sys_path = ui::GetInputPathInSys(path);

  info->path = path;
  info->evdev_id = id;
  info->connection_type = InputDataProvider::ConnectionTypeFromInputDeviceType(
      info->event_device_info.device_type());
  info->input_device = ui::InputDevice(
      id, info->event_device_info.device_type(), info->event_device_info.name(),
      info->event_device_info.phys(), sys_path,
      info->event_device_info.vendor_id(), info->event_device_info.product_id(),
      info->event_device_info.version());

  if (info->event_device_info.HasKeyboard()) {
    ui::EventRewriterChromeOS::IdentifyKeyboard(
        info->input_device, &info->keyboard_type,
        &info->keyboard_top_row_layout, &info->keyboard_scan_code_map);
  }

  return info;
}

InputDataProvider::InputDataProvider(aura::Window* window)
    : device_manager_(ui::CreateDeviceManager()),
      watcher_factory_(std::make_unique<InputDataEventWatcherFactoryImpl>()) {
  Initialize(window);
}

InputDataProvider::InputDataProvider(
    aura::Window* window,
    std::unique_ptr<ui::DeviceManager> device_manager_for_test,
    std::unique_ptr<InputDataEventWatcher::Factory> watcher_factory)
    : device_manager_(std::move(device_manager_for_test)),
      watcher_factory_(std::move(watcher_factory)) {
  Initialize(window);
}

InputDataProvider::~InputDataProvider() {
  BlockShortcuts(/*should_block=*/false);
  device_manager_->RemoveObserver(this);
  widget_->RemoveObserver(this);
}

// static
mojom::ConnectionType InputDataProvider::ConnectionTypeFromInputDeviceType(
    ui::InputDeviceType type) {
  switch (type) {
    case ui::InputDeviceType::INPUT_DEVICE_INTERNAL:
      return mojom::ConnectionType::kInternal;
    case ui::InputDeviceType::INPUT_DEVICE_USB:
      return mojom::ConnectionType::kUsb;
    case ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH:
      return mojom::ConnectionType::kBluetooth;
    case ui::InputDeviceType::INPUT_DEVICE_UNKNOWN:
      return mojom::ConnectionType::kUnknown;
  }
}

void InputDataProvider::Initialize(aura::Window* window) {
  // Window and widget are needed for security enforcement.
  CHECK(window);
  widget_ = views::Widget::GetWidgetForNativeWindow(window);
  CHECK(widget_);
  device_manager_->AddObserver(this);
  device_manager_->ScanDevices(this);
  widget_->AddObserver(this);
  UpdateMaySendEvents();
}

void InputDataProvider::BindInterface(
    mojo::PendingReceiver<mojom::InputDataProvider> pending_receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(pending_receiver));
}

void InputDataProvider::GetConnectedDevices(
    GetConnectedDevicesCallback callback) {
  std::vector<mojom::KeyboardInfoPtr> keyboard_vector;
  keyboard_vector.reserve(keyboards_.size());
  for (auto& keyboard_info : keyboards_) {
    keyboard_vector.push_back(keyboard_info.second.Clone());
  }

  std::vector<mojom::TouchDeviceInfoPtr> touch_device_vector;
  touch_device_vector.reserve(touch_devices_.size());
  for (auto& touch_device_info : touch_devices_) {
    touch_device_vector.push_back(touch_device_info.second.Clone());
  }

  base::ranges::sort(keyboard_vector, std::less<>(), &mojom::KeyboardInfo::id);
  base::ranges::sort(touch_device_vector, std::less<>(),
                     &mojom::TouchDeviceInfo::id);

  std::move(callback).Run(std::move(keyboard_vector),
                          std::move(touch_device_vector));
}

void InputDataProvider::ObserveConnectedDevices(
    mojo::PendingRemote<mojom::ConnectedDevicesObserver> observer) {
  connected_devices_observers_.Add(std::move(observer));
}

void InputDataProvider::OnWidgetVisibilityChanged(views::Widget* widget,
                                                  bool visible) {
  UpdateEventObservers();
}

void InputDataProvider::OnWidgetActivationChanged(views::Widget* widget,
                                                  bool active) {
  UpdateEventObservers();
}

void InputDataProvider::UpdateMaySendEvents() {
  const bool widget_open = !widget_->IsClosed();
  const bool widget_active = widget_->IsActive();
  const bool widget_visible = widget_->IsVisible();

  may_send_events_ = widget_open && widget_visible && widget_active;
}

void InputDataProvider::UpdateEventObservers() {
  const bool previous = may_send_events_;
  UpdateMaySendEvents();

  if (previous != may_send_events_) {
    // If there are no observers, then we never want to block shortcuts.
    // If there are observers, then we want to block when we are going to send
    // events.
    if (!keyboard_observers_.empty()) {
      BlockShortcuts(may_send_events_);
    }

    if (!may_send_events_)
      SendPauseEvents();
    else
      SendResumeEvents();
  }
}

void InputDataProvider::BlockShortcuts(bool should_block) {
  auto* accelerator_controller = Shell::Get()->accelerator_controller();
  DCHECK(accelerator_controller);
  accelerator_controller->SetPreventProcessingAccelerators(should_block);
}

void InputDataProvider::ForwardKeyboardInput(uint32_t id) {
  if (!keyboards_.contains(id)) {
    LOG(ERROR) << "Couldn't find keyboard with ID " << id
               << " when trying to forward input.";
    return;
  }

  // If we are going to send keyboard events, we need to block shortcuts
  BlockShortcuts(may_send_events_);
  keyboard_watchers_[id] =
      watcher_factory_->MakeWatcher(id, weak_factory_.GetWeakPtr());
}

void InputDataProvider::UnforwardKeyboardInput(uint32_t id) {
  if (!keyboards_.contains(id)) {
    LOG(ERROR) << "Couldn't find keyboard with ID " << id
               << " when trying to unforward input.";
  }
  if (!keyboard_watchers_.erase(id)) {
    LOG(ERROR) << "Couldn't find keyboard watcher with ID " << id
               << " when trying to unforward input.";
  }
  // If there are no more watchers, unblock shortcuts
  if (keyboard_watchers_.empty()) {
    BlockShortcuts(/*should_block=*/false);
  }
}

void InputDataProvider::OnObservedKeyboardInputDisconnect(
    uint32_t id,
    mojo::RemoteSetElementId) {
  if (!keyboard_observers_.contains(id)) {
    LOG(ERROR) << "received keyboard observer disconnect for ID " << id
               << " without observer.";
    return;
  }

  // When the last observer has been disconnected, stop forwarding events.
  if (keyboard_observers_[id]->empty()) {
    UnforwardKeyboardInput(id);

    // The observer RemoteSet remains empty at this point; if a new
    // observer comes in, we will Forward it again.
  }
}

void InputDataProvider::ObserveKeyEvents(
    uint32_t id,
    mojo::PendingRemote<mojom::KeyboardObserver> observer) {
  CHECK(widget_) << "Observing Key Events for input diagnostics not allowed "
                    "without widget to track focus.";

  if (!keyboards_.contains(id)) {
    LOG(ERROR) << "Couldn't find keyboard with ID " << id
               << " when trying to receive input.";
    return;
  }

  // When keyboard observer remote set is constructed, establish the disconnect
  // handler.
  if (!keyboard_observers_.contains(id)) {
    keyboard_observers_[id] =
        std::make_unique<mojo::RemoteSet<mojom::KeyboardObserver>>();
    keyboard_observers_[id]->set_disconnect_handler(base::BindRepeating(
        &InputDataProvider::OnObservedKeyboardInputDisconnect,
        base::Unretained(this), id));
  }

  auto& observers = *keyboard_observers_[id];

  const auto observer_id = observers.Add(std::move(observer));

  // Ensure first callback is 'Paused' if we do not currently have focus
  if (!may_send_events_)
    observers.Get(observer_id)->OnKeyEventsPaused();

  // When we are adding the first observer, start forwarding events.
  if (observers.size() == 1)
    ForwardKeyboardInput(id);
}

void InputDataProvider::SendPauseEvents() {
  for (const auto& keyboard : keyboard_observers_) {
    for (const auto& observer : *keyboard.second) {
      observer->OnKeyEventsPaused();
    }
  }

  // Re-arm our log message for future events.
  logged_not_dispatching_key_events_ = false;
}

void InputDataProvider::SendResumeEvents() {
  for (const auto& keyboard : keyboard_observers_) {
    for (const auto& observer : *keyboard.second) {
      observer->OnKeyEventsResumed();
    }
  }
}

void InputDataProvider::OnDeviceEvent(const ui::DeviceEvent& event) {
  if (event.device_type() != ui::DeviceEvent::DeviceType::INPUT ||
      event.action_type() == ui::DeviceEvent::ActionType::CHANGE) {
    return;
  }

  int id = -1;
  if (!GetEventNodeId(event.path(), &id)) {
    LOG(ERROR) << "Ignoring DeviceEvent: invalid path " << event.path();
    return;
  }

  if (event.action_type() == ui::DeviceEvent::ActionType::ADD) {
    info_helper_.AsyncCall(&InputDeviceInfoHelper::GetDeviceInfo)
        .WithArgs(id, event.path())
        .Then(base::BindOnce(&InputDataProvider::ProcessDeviceInfo,
                             weak_factory_.GetWeakPtr()));

  } else {
    DCHECK(event.action_type() == ui::DeviceEvent::ActionType::REMOVE);
    if (keyboards_.contains(id)) {
      if (keyboard_observers_.erase(id)) {
        // Unref'ing the observers does not trigger their
        // OnObservedKeyboardInputDisconnect handlers (which would normally
        // clean up any watchers), so we must explicitly release the watchers
        // here.
        keyboard_watchers_.erase(id);
      }
      keyboards_.erase(id);
      keyboard_aux_data_.erase(id);
      for (const auto& observer : connected_devices_observers_) {
        observer->OnKeyboardDisconnected(id);
      }
    } else if (touch_devices_.contains(id)) {
      touch_devices_.erase(id);
      for (const auto& observer : connected_devices_observers_) {
        observer->OnTouchDeviceDisconnected(id);
      }
    }
  }
}

InputDeviceInformation::InputDeviceInformation() = default;
InputDeviceInformation::~InputDeviceInformation() = default;

void InputDataProvider::ProcessDeviceInfo(
    std::unique_ptr<InputDeviceInformation> device_info) {
  if (device_info == nullptr) {
    return;
  }

  if (IsTouchInputDevice(device_info.get())) {
    AddTouchDevice(device_info.get());
  } else if (device_info->event_device_info.HasKeyboard()) {
    AddKeyboard(device_info.get());
  } else if (device_info->event_device_info.HasSwEvent(SW_TABLET_MODE)) {
    // Having a tablet mode switch indicates that this is a convertible, so the
    // top-right key of the keyboard is most likely to be lock.
    has_tablet_mode_switch_ = true;

    // Since this device might be processed after the internal keyboard, update
    // any internal keyboards that are already registered (except ones which we
    // know have Control Panel on the top-right key).
    for (const auto& keyboard_pair : keyboards_) {
      const mojom::KeyboardInfoPtr& keyboard = keyboard_pair.second;
      if (keyboard->connection_type == mojom::ConnectionType::kInternal &&
          keyboard->top_right_key != mojom::TopRightKey::kControlPanel) {
        keyboard->top_right_key = mojom::TopRightKey::kLock;
      }
    }
  }
}

void InputDataProvider::AddTouchDevice(
    const InputDeviceInformation* device_info) {
  touch_devices_[device_info->evdev_id] =
      touch_helper_.ConstructTouchDevice(device_info);

  for (const auto& observer : connected_devices_observers_) {
    observer->OnTouchDeviceConnected(
        touch_devices_[device_info->evdev_id]->Clone());
  }
}

void InputDataProvider::AddKeyboard(const InputDeviceInformation* device_info) {
  auto aux_data = std::make_unique<InputDataProviderKeyboard::AuxData>();

  mojom::KeyboardInfoPtr keyboard =
      keyboard_helper_.ConstructKeyboard(device_info, aux_data.get());
  if (!features::IsExternalKeyboardInDiagnosticsAppEnabled() &&
      keyboard->connection_type != mojom::ConnectionType::kInternal) {
    return;
  }
  keyboards_[device_info->evdev_id] = std::move(keyboard);
  if (device_info->connection_type == mojom::ConnectionType::kInternal &&
      keyboards_[device_info->evdev_id]->top_right_key ==
          mojom::TopRightKey::kUnknown) {
    // With some exceptions, convertibles (with tablet mode switches) tend to
    // have a lock key in the top-right of the keyboard, while clamshells tend
    // to have a power key.
    keyboards_[device_info->evdev_id]->top_right_key =
        has_tablet_mode_switch_ ? mojom::TopRightKey::kLock
                                : mojom::TopRightKey::kPower;
  }
  keyboard_aux_data_[device_info->evdev_id] = std::move(aux_data);

  for (const auto& observer : connected_devices_observers_) {
    observer->OnKeyboardConnected(keyboards_[device_info->evdev_id]->Clone());
  }
}

void InputDataProvider::SendInputKeyEvent(uint32_t id,
                                          uint32_t key_code,
                                          uint32_t scan_code,
                                          bool down) {
  CHECK(widget_) << "Sending Key Events for input diagnostics not allowed "
                    "without widget to track focus.";

  if (!keyboard_observers_.contains(id)) {
    LOG(ERROR) << "Couldn't find keyboard observer with ID " << id
               << " when trying to dispatch key.";
    return;
  }

  if (!may_send_events_) {
    if (!logged_not_dispatching_key_events_) {
      // Note: this will be common if the input diagnostics window is opened,
      // but not focused, so just log once.
      LOG(ERROR) << "Will not dispatch keys when diagnostics window does not "
                    "have focus.";
      logged_not_dispatching_key_events_ = true;
    }
    return;
  }

  mojom::KeyEventPtr event = keyboard_helper_.ConstructInputKeyEvent(
      keyboards_[id], keyboard_aux_data_[id].get(), key_code, scan_code, down);

  const auto& observers = *keyboard_observers_[id];
  for (const auto& observer : observers) {
    observer->OnKeyEvent(event->Clone());
  }
}

}  // namespace diagnostics
}  // namespace ash
