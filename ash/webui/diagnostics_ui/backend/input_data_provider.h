// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_DATA_PROVIDER_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_DATA_PROVIDER_H_

#include "ash/webui/diagnostics_ui/mojom/input_data_provider.mojom.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
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

class InputDataProvider : public mojom::InputDataProvider,
                          public ui::DeviceEventObserver {
 public:
  InputDataProvider();
  InputDataProvider(std::unique_ptr<ui::DeviceManager> device_manager);
  InputDataProvider(const InputDataProvider&) = delete;
  InputDataProvider& operator=(const InputDataProvider&) = delete;
  ~InputDataProvider() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::InputDataProvider> pending_receiver);

  // mojom::InputDataProvider:
  void GetConnectedDevices(GetConnectedDevicesCallback callback) override;

  void ObserveConnectedDevices(
      mojo::PendingRemote<mojom::ConnectedDevicesObserver> observer) override;

  // ui::DeviceEventObserver:
  void OnDeviceEvent(const ui::DeviceEvent& event) override;

 protected:
  virtual std::unique_ptr<ui::EventDeviceInfo> GetDeviceInfo(
      base::FilePath path);

 private:
  void Initialize();

  void AddTouchDevice(int id, const ui::EventDeviceInfo* device_info);
  void AddKeyboard(int id, const ui::EventDeviceInfo* device_info);

  base::flat_map<int, mojom::KeyboardInfoPtr> keyboards_;
  base::flat_map<int, mojom::TouchDeviceInfoPtr> touch_devices_;

  mojo::RemoteSet<mojom::ConnectedDevicesObserver> connected_devices_observers_;

  mojo::Receiver<mojom::InputDataProvider> receiver_{this};

  std::unique_ptr<ui::DeviceManager> device_manager_;
};

}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_DATA_PROVIDER_H_
