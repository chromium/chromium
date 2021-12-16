// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_DATA_PROVIDER_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_DATA_PROVIDER_H_

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
#include "ui/events/ozone/device/device_event.h"
#include "ui/events/ozone/device/device_event_observer.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace ash {
namespace diagnostics {

class InputDeviceInfoHelper {
 public:
  virtual ~InputDeviceInfoHelper() {}

  virtual std::unique_ptr<ui::EventDeviceInfo> GetDeviceInfo(
      base::FilePath path);
};

class InputDataProvider : public mojom::InputDataProvider,
                          public ui::DeviceEventObserver {
 public:
  InputDataProvider();
  explicit InputDataProvider(std::unique_ptr<ui::DeviceManager> device_manager);
  InputDataProvider(const InputDataProvider&) = delete;
  InputDataProvider& operator=(const InputDataProvider&) = delete;
  ~InputDataProvider() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::InputDataProvider> pending_receiver);
  // Handler for when remote attached to |receiver_| disconnects.
  void OnBoundInterfaceDisconnect();
  bool ReceiverIsBound();

  // mojom::InputDataProvider:
  void GetConnectedDevices(GetConnectedDevicesCallback callback) override;

  void ObserveConnectedDevices(
      mojo::PendingRemote<mojom::ConnectedDevicesObserver> observer) override;

  void GetKeyboardVisualLayout(
      uint32_t id,
      GetKeyboardVisualLayoutCallback callback) override;

  // ui::DeviceEventObserver:
  void OnDeviceEvent(const ui::DeviceEvent& event) override;

 protected:
  base::SequenceBound<InputDeviceInfoHelper> info_helper_{
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})};

 private:
  void Initialize();

  void ProcessDeviceInfo(int id,
                         std::unique_ptr<ui::EventDeviceInfo> device_info);

  void AddTouchDevice(int id, const ui::EventDeviceInfo* device_info);
  void AddKeyboard(int id, const ui::EventDeviceInfo* device_info);

  InputDataProviderKeyboard keyboard_helper_;
  InputDataProviderTouch touch_helper_;

  base::flat_map<int, mojom::KeyboardInfoPtr> keyboards_;
  base::flat_map<int, mojom::TouchDeviceInfoPtr> touch_devices_;

  mojo::RemoteSet<mojom::ConnectedDevicesObserver> connected_devices_observers_;

  mojo::Receiver<mojom::InputDataProvider> receiver_{this};

  std::unique_ptr<ui::DeviceManager> device_manager_;

  base::WeakPtrFactory<InputDataProvider> weak_factory_{this};
};

}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_DATA_PROVIDER_H_
