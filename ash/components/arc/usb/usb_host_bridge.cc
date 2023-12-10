// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/usb/usb_host_bridge.h"

#include <unordered_set>
#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/usb/usb_host_ui_delegate.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/strings/stringprintf.h"
#include "chromeos/dbus/permission_broker/permission_broker_client.h"
#include "content/public/browser/device_service.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace arc {
namespace {

// USB class codes are detailed at https://www.usb.org/defined-class-codes
constexpr int kUsbClassMassStorage = 0x08;

// Singleton factory for ArcUsbHostBridge
class ArcUsbHostBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcUsbHostBridge,
          ArcUsbHostBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcUsbHostBridgeFactory";

  static ArcUsbHostBridgeFactory* GetInstance() {
    return base::Singleton<ArcUsbHostBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcUsbHostBridgeFactory>;
  ArcUsbHostBridgeFactory() = default;
  ~ArcUsbHostBridgeFactory() override = default;
};

bool IsMassStorageInterface(const device::mojom::UsbInterfaceInfo& interface) {
  for (const auto& alternate : interface.alternates) {
    if (alternate->class_code == kUsbClassMassStorage)
      return true;
  }
  return false;
}

bool ShouldExposeDevice(const device::mojom::UsbDeviceInfo& device_info) {
  // ChromeOS allows mass storage devices to be detached, but we don't expose
  // these directly to ARC.
  for (const auto& configuration : device_info.configurations) {
    for (const auto& interface : configuration->interfaces) {
      if (!IsMassStorageInterface(*interface))
        return true;
    }
  }
  return false;
}

void OnDeviceOpened(mojom::UsbHostHost::OpenDeviceCallback callback,
                    base::ScopedFD fd) {
  if (!fd.is_valid()) {
    LOG(ERROR) << "Invalid USB device FD";
    std::move(callback).Run(mojo::ScopedHandle());
    return;
  }
  mojo::ScopedHandle wrapped_handle =
      mojo::WrapPlatformHandle(mojo::PlatformHandle(std::move(fd)));
  if (!wrapped_handle.is_valid()) {
    LOG(ERROR) << "Failed to wrap device FD. Closing.";
    std::move(callback).Run(mojo::ScopedHandle());
    return;
  }
  std::move(callback).Run(std::move(wrapped_handle));
}

void OnDeviceOpenError(mojom::UsbHostHost::OpenDeviceCallback callback,
                       const std::string& error_name,
                       const std::string& error_message) {
  LOG(WARNING) << "Cannot open USB device: " << error_name << ": "
               << error_message;
  std::move(callback).Run(mojo::ScopedHandle());
}

std::string GetDevicePath(const device::mojom::UsbDeviceInfo& device_info) {
  return base::StringPrintf("/dev/bus/usb/%03d/%03d", device_info.bus_number,
                            device_info.port_number);
}

}  // namespace

// static
ArcUsbHostBridge* ArcUsbHostBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcUsbHostBridgeFactory::GetForBrowserContext(context);
}

// static
ArcUsbHostBridge* ArcUsbHostBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcUsbHostBridgeFactory::GetForBrowserContextForTesting(context);
}

ArcUsbHostBridge::ArcUsbHostBridge(content::BrowserContext* context,
                                   ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->usb_host()->SetHost(this);
  arc_bridge_service_->usb_host()->AddObserver(this);
}

ArcUsbHostBridge::~ArcUsbHostBridge() {
  arc_bridge_service_->usb_host()->RemoveObserver(this);
  arc_bridge_service_->usb_host()->SetHost(nullptr);
}

BrowserContextKeyedServiceFactory* ArcUsbHostBridge::GetFactory() {
  return ArcUsbHostBridgeFactory::GetInstance();
}

void ArcUsbHostBridge::RequestPermission(const std::string& guid,
                                         const std::string& package,
                                         bool interactive,
                                         RequestPermissionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);

  if (guid.empty()) {
    HandleScanDeviceListRequest(package, std::move(callback));
    return;
  }

  VLOG(2) << "USB RequestPermission " << guid << " package " << package;

  // GUIDs are unguessable so device list should be initialized when this
  // method is being called with a valid GUID.
  auto iter = devices_.find(guid);
  if (iter == devices_.end()) {
    LOG(WARNING) << "Unknown USB device " << guid;
    std::move(callback).Run(false);
    return;
  }

  // Permission already requested.
  if (HasPermissionForDevice(*iter->second, package)) {
    std::move(callback).Run(true);
    return;
  }

  // The other side was just checking, fail without asking the user.
  if (!interactive) {
    std::move(callback).Run(false);
    return;
  }

  DCHECK(ui_delegate_);
  // Ask the authorization from the user.
  ui_delegate_->RequestUsbAccessPermission(
      package, guid, iter->second->serial_number.value_or(std::u16string()),
      iter->second->manufacturer_name.value_or(std::u16string()),
      iter->second->product_name.value_or(std::u16string()),
      iter->second->vendor_id, iter->second->product_id, std::move(callback));
}

void ArcUsbHostBridge::OpenDevice(const std::string& guid,
                                  const std::optional<std::string>& package,
                                  OpenDeviceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);

  if (!package) {
    std::move(callback).Run(mojo::ScopedHandle());
    return;
  }

  // GUIDs are unguessable so device list should be initialized when this
  // method is being called with a valid GUID.
  auto iter = devices_.find(guid);
  if (iter == devices_.end()) {
    std::move(callback).Run(mojo::ScopedHandle());
    return;
  }

  // The RequestPermission was never done, abort.
  if (!HasPermissionForDevice(*iter->second, package.value())) {
    std::move(callback).Run(mojo::ScopedHandle());
    return;
  }

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  chromeos::PermissionBrokerClient::Get()->OpenPath(
      GetDevicePath(*iter->second),
      base::BindOnce(&OnDeviceOpened, std::move(split_callback.first)),
      base::BindOnce(&OnDeviceOpenError, std::move(split_callback.second)));
}

void ArcUsbHostBridge::GetDeviceInfo(const std::string& guid,
                                     GetDeviceInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);

  // GUIDs are unguessable so device list should be initialized when this
  // method is being called with a valid GUID.
  auto iter = devices_.find(guid);
  if (iter == devices_.end()) {
    LOG(WARNING) << "Unknown USB device " << guid;
    std::move(callback).Run(std::string(), nullptr);
    return;
  }

  device::mojom::UsbDeviceInfoPtr info = iter->second->Clone();
  // b/69295049 the other side doesn't like optional strings.
  info->manufacturer_name = info->manufacturer_name.value_or(std::u16string());
  info->product_name = info->product_name.value_or(std::u16string());
  info->serial_number = info->serial_number.value_or(std::u16string());
  for (const device::mojom::UsbConfigurationInfoPtr& cfg :
       info->configurations) {
    cfg->configuration_name =
        cfg->configuration_name.value_or(std::u16string());
    for (const device::mojom::UsbInterfaceInfoPtr& iface : cfg->interfaces) {
      for (const device::mojom::UsbAlternateInterfaceInfoPtr& alt :
           iface->alternates) {
        alt->interface_name = alt->interface_name.value_or(std::u16string());
      }
    }
  }

  std::string path = GetDevicePath(*info);
  std::move(callback).Run(path, std::move(info));
}

void ArcUsbHostBridge::OnConnectionReady() {
  // Receive mojo::Remote<UsbDeviceManager> from DeviceService.
  content::GetDeviceService().BindUsbDeviceManager(
      usb_manager_.BindNewPipeAndPassReceiver());
  usb_manager_.set_disconnect_handler(
      base::BindOnce(&ArcUsbHostBridge::Disconnect, base::Unretained(this)));

  // Listen for added/removed device events.
  DCHECK(!client_receiver_.is_bound());
  usb_manager_->EnumerateDevicesAndSetClient(
      client_receiver_.BindNewEndpointAndPassRemote(),
      base::BindOnce(&ArcUsbHostBridge::InitDeviceList,
                     weak_factory_.GetWeakPtr()));
}

void ArcUsbHostBridge::OnConnectionClosed() {
  if (ui_delegate_)
    ui_delegate_->ClearPermissionRequests();

  Disconnect();
}

void ArcUsbHostBridge::Shutdown() {
  ui_delegate_ = nullptr;
}

void ArcUsbHostBridge::SetUiDelegate(ArcUsbHostUiDelegate* ui_delegate) {
  ui_delegate_ = ui_delegate;
}

void ArcUsbHostBridge::InitDeviceList(
    std::vector<device::mojom::UsbDeviceInfoPtr> devices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
  for (auto& device_info : devices) {
    DCHECK(device_info);
    std::string guid = device_info->guid;
    devices_.insert(std::make_pair(guid, std::move(device_info)));

    // Send the (filtered) list of already existing USB devices to the other
    // side.
    usb_manager_->CheckAccess(guid,
                              base::BindOnce(&ArcUsbHostBridge::OnDeviceChecked,
                                             weak_factory_.GetWeakPtr(), guid));
  }
}

std::vector<std::string> ArcUsbHostBridge::GetEventReceiverPackages(
    const device::mojom::UsbDeviceInfo& device_info) {
  DCHECK(ui_delegate_);

  std::unordered_set<std::string> receivers = ui_delegate_->GetEventPackageList(
      device_info.guid, device_info.serial_number.value_or(std::u16string()),
      device_info.vendor_id, device_info.product_id);

  return std::vector<std::string>(receivers.begin(), receivers.end());
}

void ArcUsbHostBridge::OnDeviceChecked(const std::string& guid, bool allowed) {
  if (!allowed)
    return;

  // Device can be removed between being added and returning back from
  // CheckAccess().
  auto iter = devices_.find(guid);
  if (iter == devices_.end())
    return;

  if (!ShouldExposeDevice(*iter->second))
    return;

  mojom::UsbHostInstance* usb_host_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->usb_host(), OnDeviceAdded);

  if (!usb_host_instance)
    return;

  usb_host_instance->OnDeviceAdded(guid,
                                   GetEventReceiverPackages(*iter->second));
}

bool ArcUsbHostBridge::HasPermissionForDevice(
    const device::mojom::UsbDeviceInfo& device_info,
    const std::string& package) {
  DCHECK(ui_delegate_);

  return ui_delegate_->HasUsbAccessPermission(
      package, device_info.guid,
      device_info.serial_number.value_or(std::u16string()),
      device_info.vendor_id, device_info.product_id);
}

void ArcUsbHostBridge::HandleScanDeviceListRequest(
    const std::string& package,
    RequestPermissionCallback callback) {
  DCHECK(ui_delegate_);

  VLOG(2) << "USB Request USB scan devicelist permission "
          << "package: " << package;
  ui_delegate_->RequestUsbScanDeviceListPermission(package,
                                                   std::move(callback));
}

// Disconnect the connection with the DeviceService.
void ArcUsbHostBridge::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);

  usb_manager_.reset();
  client_receiver_.reset();
  devices_.clear();
}

void ArcUsbHostBridge::OnDeviceAdded(
    device::mojom::UsbDeviceInfoPtr device_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
  DCHECK(device_info);

  // Update the device list.
  DCHECK(!base::Contains(devices_, device_info->guid));
  std::string guid = device_info->guid;
  devices_.insert(std::make_pair(guid, std::move(device_info)));

  usb_manager_->CheckAccess(guid,
                            base::BindOnce(&ArcUsbHostBridge::OnDeviceChecked,
                                           weak_factory_.GetWeakPtr(), guid));
}

void ArcUsbHostBridge::OnDeviceRemoved(
    device::mojom::UsbDeviceInfoPtr device_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
  DCHECK(device_info);

  // Update the device list.
  auto num_removed = devices_.erase(device_info->guid);
  DCHECK_EQ(num_removed, 1u);

  mojom::UsbHostInstance* usb_host_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->usb_host(), OnDeviceRemoved);

  if (!usb_host_instance) {
    VLOG(2) << "UsbInstance not ready yet";
    return;
  }

  usb_host_instance->OnDeviceRemoved(device_info->guid,
                                     GetEventReceiverPackages(*device_info));

  DCHECK(ui_delegate_);
  ui_delegate_->DeviceRemoved(device_info->guid);
}

// static
void ArcUsbHostBridge::EnsureFactoryBuilt() {
  ArcUsbHostBridgeFactory::GetInstance();
}

}  // namespace arc
