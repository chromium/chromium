// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_usb_host_permission_manager.h"

#include <utility>
#include <vector>

#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/usb/usb_host_bridge.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_list/arc/arc_app_dialog.h"
#include "chrome/browser/ash/app_list/arc/arc_usb_host_permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/api/device_permissions_manager.h"

namespace arc {

namespace {

constexpr char kUsbAccessPermission[] = "usb_access_permission";
constexpr char kUsbDeviceName[] = "usb_device_name";
constexpr char kUsbProductId[] = "usb_product_id";
constexpr char kUsbScanDeviceListPermission[] =
    "usb_scan_devicelist_permission";
constexpr char kUsbSerialNumber[] = "usb_serial_number";
constexpr char kUsbVendorId[] = "usb_vendor_id";

std::string GetAppIdFromPackageName(const std::string& package_name,
                                    const ArcAppListPrefs* arc_app_list_prefs) {
  DCHECK(arc_app_list_prefs);

  // For app icon and app name in UI dialog, find a matching launchable activity
  // for the requesting package. If there are multiple launchable activities
  // from the package, use the app icon from first found matching launchable
  // activity in the permission dialog.
  std::unordered_set<std::string> app_ids =
      arc_app_list_prefs->GetAppsForPackage(package_name);
  return app_ids.empty() ? std::string() : *app_ids.begin();
}

bool GetUint16FromDict(const base::Value::Dict& dict,
                       const std::string& key,
                       uint16_t* result) {
  std::optional<int> value = dict.FindInt(key);
  if (!value.has_value()) {
    return false;
  }
  int tmp = *value;
  if (tmp < 0 || tmp > UINT16_MAX)
    return false;
  *result = static_cast<uint16_t>(tmp);
  return true;
}

}  // namespace

// UsbPermissionRequest
ArcUsbHostPermissionManager::UsbPermissionRequest::UsbPermissionRequest(
    const std::string& package_name,
    bool is_scan_request,
    std::optional<UsbDeviceEntry> usb_device_entry,
    std::optional<ArcUsbHostUiDelegate::RequestPermissionCallback> callback)
    : package_name_(package_name),
      is_scan_request_(is_scan_request),
      usb_device_entry_(std::move(usb_device_entry)),
      callback_(std::move(callback)) {}

ArcUsbHostPermissionManager::UsbPermissionRequest::UsbPermissionRequest(
    ArcUsbHostPermissionManager::UsbPermissionRequest&& other) = default;

ArcUsbHostPermissionManager::UsbPermissionRequest&
ArcUsbHostPermissionManager::UsbPermissionRequest::operator=(
    ArcUsbHostPermissionManager::UsbPermissionRequest&& other) = default;

ArcUsbHostPermissionManager::UsbPermissionRequest::~UsbPermissionRequest() =
    default;

void ArcUsbHostPermissionManager::UsbPermissionRequest::Resolve(bool allowed) {
  if (!callback_)
    return;
  std::move(*callback_).Run(allowed);
}

// UsbDeviceEntry
ArcUsbHostPermissionManager::UsbDeviceEntry::UsbDeviceEntry(
    const std::string& guid,
    const std::u16string& device_name,
    const std::u16string& serial_number,
    uint16_t vendor_id,
    uint16_t product_id)
    : guid(guid),
      device_name(device_name),
      serial_number(serial_number),
      vendor_id(vendor_id),
      product_id(product_id) {}

ArcUsbHostPermissionManager::UsbDeviceEntry::UsbDeviceEntry(
    const ArcUsbHostPermissionManager::UsbDeviceEntry& other) = default;

ArcUsbHostPermissionManager::UsbDeviceEntry&
ArcUsbHostPermissionManager::UsbDeviceEntry::operator=(
    const ArcUsbHostPermissionManager::UsbDeviceEntry& other) = default;

bool ArcUsbHostPermissionManager::UsbDeviceEntry::Matches(
    const UsbDeviceEntry& other) const {
  if (IsPersistent() && other.IsPersistent()) {
    return serial_number == other.serial_number &&
           vendor_id == other.vendor_id && product_id == other.product_id;
  } else {
    return guid == other.guid;
  }
}

// static
ArcUsbHostPermissionManager* ArcUsbHostPermissionManager::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcUsbHostPermissionManagerFactory::GetForBrowserContext(context);
}

// static
std::unique_ptr<ArcUsbHostPermissionManager>
ArcUsbHostPermissionManager::Create(content::BrowserContext* context) {
  ArcAppListPrefs* arc_app_list_prefs = ArcAppListPrefs::Get(context);
  // TODO(lgcheng): Change this to DCHECK(arc_app_list_prefs) after clear the
  // browsertest workflow.
  if (!arc_app_list_prefs)
    return nullptr;

  ArcUsbHostBridge* arc_usb_host_bridge =
      ArcUsbHostBridge::GetForBrowserContext(context);
  if (!arc_usb_host_bridge)
    return nullptr;

  return std::make_unique<ArcUsbHostPermissionManager>(
      static_cast<Profile*>(context), arc_app_list_prefs, arc_usb_host_bridge);
}

ArcUsbHostPermissionManager::ArcUsbHostPermissionManager(
    Profile* profile,
    ArcAppListPrefs* arc_app_list_prefs,
    ArcUsbHostBridge* arc_usb_host_bridge)
    : profile_(profile), arc_app_list_prefs_(arc_app_list_prefs) {
  RestorePermissionFromChromePrefs();
  arc_app_list_prefs_->AddObserver(this);
  arc_usb_host_bridge->SetUiDelegate(this);
}

ArcUsbHostPermissionManager::~ArcUsbHostPermissionManager() {
  arc_app_list_prefs_->RemoveObserver(this);
}

void ArcUsbHostPermissionManager::RestorePermissionFromChromePrefs() {
  const std::vector<std::string> packages =
      arc_app_list_prefs_->GetPackagesFromPrefs();
  for (auto& package : packages) {
    // Restore scan device list permission.
    const base::Value* scan_permission_value =
        arc_app_list_prefs_->GetPackagePrefs(package,
                                             kUsbScanDeviceListPermission);
    if (scan_permission_value && scan_permission_value->is_bool() &&
        scan_permission_value->GetBool()) {
      usb_scan_devicelist_permission_packages_.emplace(package);
    }

    // Restore device access permission.
    const base::Value* access_permission_list_value =
        arc_app_list_prefs_->GetPackagePrefs(package, kUsbAccessPermission);
    if (!access_permission_list_value) {
      continue;
    }

    if (!access_permission_list_value->is_list()) {
      LOG(ERROR) << "Wrong value type found for device access permission list.";
      continue;
    }

    for (const auto& access_permission_value :
         access_permission_list_value->GetList()) {
      const base::Value::Dict& access_permission =
          access_permission_value.GetDict();
      const std::string* serial_number =
          access_permission.FindString(kUsbSerialNumber);
      if (!serial_number) {
        LOG(WARNING) << "Invalid device access permission: No serial number.";
        continue;
      }

      const std::string* device_name =
          access_permission.FindString(kUsbDeviceName);
      if (!device_name) {
        LOG(WARNING) << "Invalid device access permission: No device name.";
        continue;
      }

      uint16_t vendor_id;
      uint16_t product_id;

      if (!GetUint16FromDict(access_permission, kUsbVendorId, &vendor_id) ||
          !GetUint16FromDict(access_permission, kUsbProductId, &product_id)) {
        LOG(WARNING) << "Invalid device access permission: Invalid vendor_id "
                        "and/or product_id.";
        continue;
      }

      usb_access_permission_dict_.emplace(
          package, UsbDeviceEntry(std::string() /*guid*/,
                                  base::UTF8ToUTF16(*device_name),
                                  base::UTF8ToUTF16(*serial_number), vendor_id,
                                  product_id));
    }
  }
}

void ArcUsbHostPermissionManager::RequestUsbScanDeviceListPermission(
    const std::string& package_name,
    ArcUsbHostUiDelegate::RequestPermissionCallback callback) {
  if (HasUsbScanDeviceListPermission(package_name)) {
    std::move(callback).Run(true);
    return;
  }

  // Return ASAP to stop app from ANR. If granted, the permission will apply
  // when next time app tries to get device list.
  std::move(callback).Run(false);
  pending_requests_.emplace_back(
      ArcUsbHostPermissionManager::UsbPermissionRequest(
          package_name, true /*is_scan_request*/,
          std::nullopt /*usb_device_entry*/, std::nullopt /*callback*/));
  MaybeProcessNextPermissionRequest();
}

void ArcUsbHostPermissionManager::RequestUsbAccessPermission(
    const std::string& package_name,
    const std::string& guid,
    const std::u16string& serial_number,
    const std::u16string& manufacturer_string,
    const std::u16string& product_string,
    uint16_t vendor_id,
    uint16_t product_id,
    ArcUsbHostUiDelegate::RequestPermissionCallback callback) {
  UsbDeviceEntry usb_device_entry(
      guid,
      extensions::DevicePermissionsManager::GetPermissionMessage(
          vendor_id, product_id, manufacturer_string, product_string,
          serial_number, true /*always_include_manufacturer*/),
      serial_number, vendor_id, product_id);

  if (HasUsbAccessPermission(package_name, usb_device_entry)) {
    std::move(callback).Run(true);
    return;
  }
  pending_requests_.emplace_back(
      ArcUsbHostPermissionManager::UsbPermissionRequest(
          package_name, false /*is_scan_request*/, std::move(usb_device_entry),
          std::move(callback)));
  MaybeProcessNextPermissionRequest();
}

bool ArcUsbHostPermissionManager::HasUsbAccessPermission(
    const std::string& package_name,
    const std::string& guid,
    const std::u16string& serial_number,
    uint16_t vendor_id,
    uint16_t product_id) const {
  UsbDeviceEntry usb_device_entry(guid, std::u16string() /*device_name*/,
                                  serial_number, vendor_id, product_id);
  return HasUsbAccessPermission(package_name, usb_device_entry);
}

void ArcUsbHostPermissionManager::GrantUsbAccessPermission(
    const std::string& package_name,
    const std::string& guid,
    uint16_t vendor_id,
    uint16_t product_id) {
  // Create non-persistent usb device entry with empty serial_number.
  UsbDeviceEntry usb_device_entry(guid, std::u16string() /*device_name*/,
                                  std::u16string() /*serial_number*/, vendor_id,
                                  product_id);
  DCHECK(!usb_device_entry.IsPersistent());
  UpdateArcUsbAccessPermission(package_name, usb_device_entry,
                               true /*allowed*/);
}

std::unordered_set<std::string>
ArcUsbHostPermissionManager::GetEventPackageList(
    const std::string& guid,
    const std::u16string& serial_number,
    uint16_t vendor_id,
    uint16_t product_id) const {
  // Packages with USB device scan permission should receive USB events.
  std::unordered_set<std::string> event_packages(
      usb_scan_devicelist_permission_packages_);

  // Packages have USB access permission to this device should receive USB
  // events for this USB device.
  UsbDeviceEntry usb_device_entry(guid, std::u16string() /*device_name*/,
                                  serial_number, vendor_id, product_id);
  for (const auto& entry : usb_access_permission_dict_) {
    if (entry.second.Matches(usb_device_entry))
      event_packages.emplace(entry.first);
  }
  return event_packages;
}

void ArcUsbHostPermissionManager::DeviceRemoved(const std::string& guid) {
  // Remove pending requests.
  std::erase_if(pending_requests_,
                [guid](const UsbPermissionRequest& usb_permission_request) {
                  return !usb_permission_request.is_scan_request() &&
                         usb_permission_request.usb_device_entry()->guid ==
                             guid;
                });
  // Remove runtime permissions.
  for (auto iter = usb_access_permission_dict_.begin();
       iter != usb_access_permission_dict_.end();) {
    UsbDeviceEntry& usb_device_entry = iter->second;
    if (!usb_device_entry.IsPersistent() && usb_device_entry.guid == guid)
      iter = usb_access_permission_dict_.erase(iter);
    else
      ++iter;
  }
  if (current_requesting_guid_ == guid)
    current_requesting_guid_.clear();
}

void ArcUsbHostPermissionManager::OnPackageRemoved(
    const std::string& package_name,
    bool uninstalled) {
  // Remove pending requests.
  std::erase_if(
      pending_requests_,
      [package_name](const UsbPermissionRequest& usb_permission_request) {
        return usb_permission_request.package_name() == package_name;
      });
  // Remove runtime permissions.
  usb_scan_devicelist_permission_packages_.erase(package_name);
  usb_access_permission_dict_.erase(package_name);
  if (current_requesting_package_ == package_name)
    current_requesting_package_.clear();
}

void ArcUsbHostPermissionManager::MaybeProcessNextPermissionRequest() {
  if (is_permission_dialog_visible_ || pending_requests_.empty())
    return;

  is_permission_dialog_visible_ = true;

  auto current_request = std::move(pending_requests_.front());
  pending_requests_.erase(pending_requests_.begin());
  current_requesting_package_ = current_request.package_name();

  if (current_request.is_scan_request())
    current_requesting_guid_.clear();
  else
    current_requesting_guid_ = current_request.usb_device_entry()->guid;

  const std::string app_id =
      GetAppIdFromPackageName(current_requesting_package_, arc_app_list_prefs_);
  // App is uninstalled during the process.
  if (app_id.empty()) {
    OnUsbPermissionReceived(std::move(current_request), false);
    return;
  }

  const std::string app_name = arc_app_list_prefs_->GetApp(app_id)->name;

  if (current_request.is_scan_request()) {
    if (HasUsbScanDeviceListPermission(current_requesting_package_)) {
      OnUsbPermissionReceived(std::move(current_request), true);
    } else {
      ShowUsbScanDeviceListPermissionDialog(
          profile_, app_id,
          base::BindOnce(&ArcUsbHostPermissionManager::OnUsbPermissionReceived,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(current_request)));
    }
  } else {
    if (HasUsbAccessPermission(current_requesting_package_,
                               *current_request.usb_device_entry())) {
      OnUsbPermissionReceived(std::move(current_request), true);
    } else {
      const std::u16string device_name =
          current_request.usb_device_entry()->device_name;
      ShowUsbAccessPermissionDialog(
          profile_, app_id, device_name,
          base::BindOnce(&ArcUsbHostPermissionManager::OnUsbPermissionReceived,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(current_request)));
    }
  }
}

bool ArcUsbHostPermissionManager::HasUsbScanDeviceListPermission(
    const std::string& package_name) const {
  return usb_scan_devicelist_permission_packages_.count(package_name);
}

bool ArcUsbHostPermissionManager::HasUsbAccessPermission(
    const std::string& package_name,
    const UsbDeviceEntry& usb_device_entry) const {
  auto range = usb_access_permission_dict_.equal_range(package_name);
  for (auto iter = range.first; iter != range.second; iter++) {
    if (iter->second.Matches(usb_device_entry))
      return true;
  }
  return false;
}

void ArcUsbHostPermissionManager::ClearPermissionRequests() {
  pending_requests_.clear();
  current_requesting_package_.clear();
  current_requesting_guid_.clear();
  is_permission_dialog_visible_ = false;
}

void ArcUsbHostPermissionManager::OnUsbPermissionReceived(
    UsbPermissionRequest request,
    bool allowed) {
  is_permission_dialog_visible_ = false;

  const std::string& package_name = request.package_name();

  // If the package can is uninstalled while user clicks permission UI dialog.
  // Ignore current callback as it's outdated.
  if (package_name != current_requesting_package_) {
    MaybeProcessNextPermissionRequest();
    return;
  }

  // If the USB device is removed while user clicks permission UI dialog.
  // Ignore current callback as it's outdated.
  if (!request.is_scan_request() &&
      request.usb_device_entry()->guid != current_requesting_guid_) {
    MaybeProcessNextPermissionRequest();
    return;
  }

  if (request.is_scan_request()) {
    UpdateArcUsbScanDeviceListPermission(package_name, allowed);
  } else {
    request.Resolve(allowed);
    UpdateArcUsbAccessPermission(package_name, *request.usb_device_entry(),
                                 allowed);
  }

  MaybeProcessNextPermissionRequest();
}

void ArcUsbHostPermissionManager::UpdateArcUsbScanDeviceListPermission(
    const std::string& package_name,
    bool allowed) {
  // Currently we don't keep denied request. But keep the option open.
  if (!allowed)
    return;

  const auto result =
      usb_scan_devicelist_permission_packages_.emplace(package_name);
  // Permission is already recored.
  if (!result.second)
    return;

  arc_app_list_prefs_->SetPackagePrefs(
      package_name, kUsbScanDeviceListPermission, base::Value(allowed));
}

void ArcUsbHostPermissionManager::UpdateArcUsbAccessPermission(
    const std::string& package_name,
    const UsbDeviceEntry& usb_device_entry,
    bool allowed) {
  // Currently we don't keep denied request. But keep the option open.
  if (!allowed)
    return;

  // Record already exists.
  if (HasUsbAccessPermission(package_name, usb_device_entry))
    return;

  usb_access_permission_dict_.emplace(
      std::make_pair(package_name, usb_device_entry));

  if (!usb_device_entry.IsPersistent())
    return;

  base::Value::Dict new_permission;
  new_permission.Set(kUsbSerialNumber, usb_device_entry.serial_number);
  new_permission.Set(kUsbDeviceName, usb_device_entry.device_name);
  new_permission.Set(kUsbVendorId, usb_device_entry.vendor_id);
  new_permission.Set(kUsbProductId, usb_device_entry.product_id);

  base::Value* access_permission_list_value =
      arc_app_list_prefs_->GetPackagePrefs(package_name, kUsbAccessPermission);
  if (!access_permission_list_value ||
      !access_permission_list_value->is_list()) {
    if (access_permission_list_value &&
        !access_permission_list_value->is_list()) {
      LOG(WARNING) << "Wrong base::Value::Type found for device access "
                      "permission list Chrome prefs. Will overwrite with "
                      "base::Value::Type::List";
    }
    arc_app_list_prefs_->SetPackagePrefs(package_name, kUsbAccessPermission,
                                         base::Value(base::Value::Type::LIST));
  }
  arc_app_list_prefs_->GetPackagePrefs(package_name, kUsbAccessPermission)
      ->GetList()
      .Append(std::move(new_permission));
}

void ArcUsbHostPermissionManager::ClearPermissionForTesting() {
  usb_scan_devicelist_permission_packages_.clear();
  usb_access_permission_dict_.clear();
}

}  // namespace arc
