// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_HID_HID_IMPL_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_HID_HID_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "third_party/blink/public/mojom/chromeos/system_extensions/hid/cros_hid.mojom.h"
#include "third_party/blink/public/mojom/hid/hid.mojom-forward.h"

class Profile;

namespace content {
struct ServiceWorkerVersionBaseInfo;
}

namespace ash {

class HIDImpl : public blink::mojom::CrosHID,
                public device::mojom::HidManagerClient,
                public device::mojom::HidConnectionWatcher {
 public:
  static void Bind(Profile* profile,
                   const content::ServiceWorkerVersionBaseInfo& info,
                   mojo::PendingReceiver<blink::mojom::CrosHID> receiver);

  HIDImpl();
  ~HIDImpl() override;

  void AccessDevices(std::vector<blink::mojom::HidDeviceFilterPtr> filters,
                     AccessDevicesCallback callback) override;

  // device::mojom::HidManagerClient implementation:
  void DeviceAdded(device::mojom::HidDeviceInfoPtr device_info) override;
  void DeviceRemoved(device::mojom::HidDeviceInfoPtr device_info) override;
  void DeviceChanged(device::mojom::HidDeviceInfoPtr device_info) override;

  device::mojom::HidManager* GetHidManager();

  void Connect(const std::string& device_guid,
               mojo::PendingRemote<device::mojom::HidConnectionClient> client,
               ConnectCallback callback) override;

 private:
  void OnGotDevices(std::vector<blink::mojom::HidDeviceFilterPtr> filters,
                    AccessDevicesCallback callback,
                    std::vector<device::mojom::HidDeviceInfoPtr> devices);
  bool FilterMatchesAny(
      const device::mojom::HidDeviceInfo& device,
      const std::vector<blink::mojom::HidDeviceFilterPtr>& filters) const;

  void EnsureHidManagerConnection();
  void SetUpHidManagerConnection(
      mojo::PendingRemote<device::mojom::HidManager> manager);
  void InitDeviceList(std::vector<device::mojom::HidDeviceInfoPtr> devices);

  mojo::Remote<device::mojom::HidManager> hid_manager_remote_;
  mojo::AssociatedReceiver<device::mojom::HidManagerClient>
      hid_manager_client_associated_receiver_{this};

  // Map from device GUID to device info.
  std::map<std::string, device::mojom::HidDeviceInfoPtr> device_map_;
  bool device_map_is_initialized_ = false;

  // Each pipe here watches a connection created by Connect().
  mojo::ReceiverSet<device::mojom::HidConnectionWatcher> watchers_;

  // Last member definition.
  base::WeakPtrFactory<HIDImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_HID_HID_IMPL_H_
