// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/usb/usb_device_manager_helper.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/system_connector.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

using device::mojom::UsbTransferDirection;
using device::mojom::UsbTransferType;

namespace {

const int kAdbClass = 0xff;
const int kAdbSubclass = 0x42;
const int kAdbProtocol = 0x1;

std::unique_ptr<AndroidInterfaceInfo> FindAndroidInterface(
    const device::mojom::UsbDeviceInfo& device_info) {
  if (!device_info.active_configuration)
    return nullptr;

  // Find the active configuration object.
  device::mojom::UsbConfigurationInfo* active_config = nullptr;
  for (const auto& config : device_info.configurations) {
    if (config->configuration_value == device_info.active_configuration) {
      active_config = config.get();
      break;
    }
  }
  if (!active_config)
    return nullptr;

  for (const auto& interface : active_config->interfaces) {
    for (const auto& alternate : interface->alternates) {
      if (alternate->alternate_setting == 0 &&
          alternate->class_code == kAdbClass &&
          alternate->subclass_code == kAdbSubclass &&
          alternate->protocol_code == kAdbProtocol &&
          alternate->endpoints.size() == 2) {
        return std::make_unique<AndroidInterfaceInfo>(
            interface->interface_number, alternate.get());
      }
    }
  }
  return nullptr;
}

void GetAndroidDeviceInfoList(
    AndroidDeviceInfoListCallback callback,
    std::vector<device::mojom::UsbDeviceInfoPtr> usb_devices) {
  std::vector<AndroidDeviceInfo> result;
  for (auto& device_info : usb_devices) {
    if (!device_info->serial_number || device_info->serial_number->empty())
      continue;

    auto interface_info = FindAndroidInterface(*device_info);
    if (!interface_info)
      continue;

    int inbound_address = 0;
    int outbound_address = 0;
    int zero_mask = 0;

    for (auto& endpoint : interface_info->alternate->endpoints) {
      if (endpoint->type != UsbTransferType::BULK)
        continue;
      if (endpoint->direction == UsbTransferDirection::INBOUND)
        inbound_address = endpoint->endpoint_number;
      else
        outbound_address = endpoint->endpoint_number;
      zero_mask = endpoint->packet_size - 1;
    }

    if (inbound_address == 0 || outbound_address == 0)
      continue;

    result.push_back(AndroidDeviceInfo(
        device_info->guid,
        base::UTF16ToASCII(device_info->serial_number.value()),
        interface_info->interface_number, inbound_address, outbound_address,
        zero_mask));
  }

  std::move(callback).Run(std::move(result));
}

void CountAndroidDevices(base::OnceCallback<void(int)> callback,
                         std::vector<AndroidDeviceInfo> info_list) {
  std::move(callback).Run(info_list.size());
}

void BindDeviceServiceOnUIThread(
    mojo::PendingReceiver<device::mojom::UsbDeviceManager> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Bind to the DeviceService for USB device manager.
  content::GetSystemConnector()->Connect(device::mojom::kServiceName,
                                         std::move(receiver));
}

}  // namespace

AndroidInterfaceInfo::AndroidInterfaceInfo(
    uint8_t interface_number,
    const device::mojom::UsbAlternateInterfaceInfo* alternate)
    : interface_number(interface_number), alternate(alternate) {}

AndroidDeviceInfo::AndroidDeviceInfo(const std::string& guid,
                                     const std::string& serial,
                                     int interface_id,
                                     int inbound_address,
                                     int outbound_address,
                                     int zero_mask)
    : guid(guid),
      serial(serial),
      interface_id(interface_id),
      inbound_address(inbound_address),
      outbound_address(outbound_address),
      zero_mask(zero_mask) {}

AndroidDeviceInfo::AndroidDeviceInfo(const AndroidDeviceInfo& other) = default;

// static
UsbDeviceManagerHelper* UsbDeviceManagerHelper::GetInstance() {
  static base::NoDestructor<UsbDeviceManagerHelper> s_instance;
  return s_instance.get();
}

// static
void UsbDeviceManagerHelper::CountDevices(
    base::OnceCallback<void(int)> callback) {
  GetInstance()->CountDevicesInternal(std::move(callback));
}

// static
void UsbDeviceManagerHelper::SetUsbManagerForTesting(
    mojo::PendingRemote<device::mojom::UsbDeviceManager> fake_usb_manager) {
  GetInstance()->SetUsbManagerForTestingInternal(std::move(fake_usb_manager));
}

UsbDeviceManagerHelper::UsbDeviceManagerHelper() {}

UsbDeviceManagerHelper::~UsbDeviceManagerHelper() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void UsbDeviceManagerHelper::GetAndroidDevices(
    AndroidDeviceInfoListCallback callback) {
  EnsureUsbDeviceManagerConnection();

  DCHECK(device_manager_);
  device_manager_->GetDevices(
      /*options=*/nullptr,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&GetAndroidDeviceInfoList, std::move(callback)),
          std::vector<device::mojom::UsbDeviceInfoPtr>()));
}

void UsbDeviceManagerHelper::GetDevice(
    const std::string& guid,
    mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver) {
  EnsureUsbDeviceManagerConnection();

  DCHECK(device_manager_);
  device_manager_->GetDevice(guid, std::move(device_receiver),
                             /*device_client=*/mojo::NullRemote());
}

void UsbDeviceManagerHelper::EnsureUsbDeviceManagerConnection() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Ensure connection with the Device Service.
  if (device_manager_)
    return;

  // Just for testing.
  if (testing_device_manager_) {
    device_manager_.Bind(std::move(testing_device_manager_));
    device_manager_.set_disconnect_handler(
        base::BindOnce(&UsbDeviceManagerHelper::OnDeviceManagerConnectionError,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&BindDeviceServiceOnUIThread,
                                device_manager_.BindNewPipeAndPassReceiver()));

  device_manager_.set_disconnect_handler(
      base::BindOnce(&UsbDeviceManagerHelper::OnDeviceManagerConnectionError,
                     weak_factory_.GetWeakPtr()));
}

void UsbDeviceManagerHelper::CountDevicesInternal(
    base::OnceCallback<void(int)> callback) {
  EnsureUsbDeviceManagerConnection();

  DCHECK(device_manager_);
  auto countCb = base::BindOnce(&CountAndroidDevices, std::move(callback));
  device_manager_->GetDevices(
      /*options=*/nullptr,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&GetAndroidDeviceInfoList, std::move(countCb)),
          std::vector<device::mojom::UsbDeviceInfoPtr>()));
}

void UsbDeviceManagerHelper::SetUsbManagerForTestingInternal(
    mojo::PendingRemote<device::mojom::UsbDeviceManager> fake_usb_manager) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(fake_usb_manager);
  testing_device_manager_ = std::move(fake_usb_manager);
}

void UsbDeviceManagerHelper::OnDeviceManagerConnectionError() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  device_manager_.reset();
}
