// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/api/hid/hid_impl.h"

#include <utility>

#include "base/ranges/algorithm.h"
#include "content/public/browser/device_service.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/device/hid/hid_service.h"
#include "third_party/blink/public/mojom/hid/hid.mojom.h"

namespace ash {

namespace {

void OnConnectResponse(
    HIDImpl::ConnectCallback callback,
    mojo::PendingRemote<device::mojom::HidConnection> connection) {
  if (!connection) {
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  std::move(callback).Run(std::move(connection));
}

}  // namespace

// static
void HIDImpl::Bind(Profile* profile,
                   const content::ServiceWorkerVersionBaseInfo& info,
                   mojo::PendingReceiver<blink::mojom::CrosHID> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<HIDImpl>(), std::move(receiver));
}

HIDImpl::HIDImpl() = default;

HIDImpl::~HIDImpl() = default;

void HIDImpl::AccessDevices(
    std::vector<blink::mojom::HidDeviceFilterPtr> filters,
    AccessDevicesCallback callback) {
  GetHidManager()->GetDevices(
      base::BindOnce(&HIDImpl::OnGotDevices, weak_factory_.GetWeakPtr(),
                     std::move(filters), std::move(callback)));
}

void HIDImpl::OnGotDevices(
    std::vector<blink::mojom::HidDeviceFilterPtr> filters,
    AccessDevicesCallback callback,
    std::vector<device::mojom::HidDeviceInfoPtr> devices) {
  std::vector<device::mojom::HidDeviceInfoPtr> filtered_devices;
  for (auto& device : devices) {
    if (FilterMatchesAny(*device, filters))
      filtered_devices.push_back(std::move(device));
  }

  std::move(callback).Run(std::move(filtered_devices));
}

bool HIDImpl::FilterMatchesAny(
    const device::mojom::HidDeviceInfo& device,
    const std::vector<blink::mojom::HidDeviceFilterPtr>& filters) const {
  // TODO(b/216239205): Reuse HidChooserController::FilterMatchesAny after
  // refactoring it.

  if (filters.empty())
    return true;

  for (const auto& filter : filters) {
    if (filter->device_ids) {
      if (filter->device_ids->is_vendor()) {
        if (filter->device_ids->get_vendor() != device.vendor_id)
          continue;
      } else if (filter->device_ids->is_vendor_and_product()) {
        const auto& vendor_and_product =
            filter->device_ids->get_vendor_and_product();
        if (vendor_and_product->vendor != device.vendor_id)
          continue;
        if (vendor_and_product->product != device.product_id)
          continue;
      }
    }

    if (filter->usage) {
      if (filter->usage->is_page()) {
        const uint16_t usage_page = filter->usage->get_page();
        if (!base::ranges::any_of(
                device.collections,
                [&usage_page](const device::mojom::HidCollectionInfoPtr& c) {
                  return usage_page == c->usage->usage_page;
                })) {
          continue;
        }
      } else if (filter->usage->is_usage_and_page()) {
        const auto& usage_and_page = filter->usage->get_usage_and_page();
        if (!base::ranges::any_of(
                device.collections,
                [&usage_and_page](
                    const device::mojom::HidCollectionInfoPtr& c) {
                  return usage_and_page->usage_page == c->usage->usage_page &&
                         usage_and_page->usage == c->usage->usage;
                  ;
                })) {
          continue;
        }
      }
    }

    return true;
  }

  return false;
}

void HIDImpl::EnsureHidManagerConnection() {
  if (hid_manager_remote_)
    return;

  mojo::PendingRemote<device::mojom::HidManager> manager;
  content::GetDeviceService().BindHidManager(
      manager.InitWithNewPipeAndPassReceiver());
  SetUpHidManagerConnection(std::move(manager));
}

void HIDImpl::SetUpHidManagerConnection(
    mojo::PendingRemote<device::mojom::HidManager> manager) {
  hid_manager_remote_.Bind(std::move(manager));

  hid_manager_remote_->GetDevicesAndSetClient(
      hid_manager_client_associated_receiver_.BindNewEndpointAndPassRemote(),
      base::BindOnce(&HIDImpl::InitDeviceList, weak_factory_.GetWeakPtr()));
}

void HIDImpl::InitDeviceList(
    std::vector<device::mojom::HidDeviceInfoPtr> devices) {
  for (auto& device : devices)
    device_map_.insert({device->guid, std::move(device)});

  device_map_is_initialized_ = true;
}

device::mojom::HidManager* HIDImpl::GetHidManager() {
  EnsureHidManagerConnection();
  return hid_manager_remote_.get();
}

void HIDImpl::Connect(
    const std::string& device_guid,
    mojo::PendingRemote<device::mojom::HidConnectionClient> client,
    ConnectCallback callback) {
  mojo::PendingRemote<device::mojom::HidConnectionWatcher> watcher;
  watchers_.Add(this, watcher.InitWithNewPipeAndPassReceiver());

  GetHidManager()->Connect(
      device_guid, std::move(client), std::move(watcher),
      /*allow_protected_reports=*/false,
      /*allow_fido_reports=*/false,
      base::BindOnce(&OnConnectResponse, std::move(callback)));
}

void HIDImpl::DeviceAdded(device::mojom::HidDeviceInfoPtr device_info) {}
void HIDImpl::DeviceRemoved(device::mojom::HidDeviceInfoPtr device_info) {}
void HIDImpl::DeviceChanged(device::mojom::HidDeviceInfoPtr device_info) {}

}  // namespace ash
