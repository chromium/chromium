// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_chooser_context.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_blocklist.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/device_service.h"

namespace {

constexpr char kHidDeviceNameKey[] = "name";
constexpr char kHidGuidKey[] = "guid";
constexpr char kHidVendorIdKey[] = "vendor-id";
constexpr char kHidProductIdKey[] = "product-id";
constexpr char kHidSerialNumberKey[] = "serial-number";

bool CanStorePersistentEntry(const device::mojom::HidDeviceInfo& device) {
  return !device.serial_number.empty();
}

base::Value DeviceInfoToValue(const device::mojom::HidDeviceInfo& device) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetStringKey(kHidDeviceNameKey, device.product_name);
  value.SetIntKey(kHidVendorIdKey, device.vendor_id);
  value.SetIntKey(kHidProductIdKey, device.product_id);
  if (CanStorePersistentEntry(device)) {
    // Use the USB serial number as a persistent identifier. If it is
    // unavailable, only ephemeral permissions may be granted.
    value.SetStringKey(kHidSerialNumberKey, device.serial_number);
  } else {
    // The GUID is a temporary ID created on connection that remains valid until
    // the device is disconnected. Ephemeral permissions are keyed by this ID
    // and must be granted again each time the device is connected.
    value.SetStringKey(kHidGuidKey, device.guid);
  }
  return value;
}

}  // namespace

HidChooserContext::HidChooserContext(Profile* profile)
    : ChooserContextBase(ContentSettingsType::HID_GUARD,
                         ContentSettingsType::HID_CHOOSER_DATA,
                         HostContentSettingsMapFactory::GetForProfile(profile)),
      is_incognito_(profile->IsOffTheRecord()) {}

HidChooserContext::~HidChooserContext() = default;

base::string16 HidChooserContext::GetObjectDisplayName(
    const base::Value& object) {
  const std::string* name = object.FindStringKey(kHidDeviceNameKey);
  DCHECK(name);
  return base::UTF8ToUTF16(*name);
}

bool HidChooserContext::IsValidObject(const base::Value& object) {
  if (!object.is_dict() || object.DictSize() != 4 ||
      !object.FindStringKey(kHidDeviceNameKey) ||
      !object.FindIntKey(kHidProductIdKey) ||
      !object.FindIntKey(kHidVendorIdKey)) {
    return false;
  }

  const std::string* guid = object.FindStringKey(kHidGuidKey);
  const std::string* serial_number = object.FindStringKey(kHidSerialNumberKey);
  return (guid && !guid->empty()) || (serial_number && !serial_number->empty());
}

std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
HidChooserContext::GetGrantedObjects(const url::Origin& requesting_origin,
                                     const url::Origin& embedding_origin) {
  std::vector<std::unique_ptr<ChooserContextBase::Object>> objects =
      ChooserContextBase::GetGrantedObjects(requesting_origin,
                                            embedding_origin);

  if (CanRequestObjectPermission(requesting_origin, embedding_origin)) {
    auto it = ephemeral_devices_.find(
        std::make_pair(requesting_origin, embedding_origin));
    if (it != ephemeral_devices_.end()) {
      for (const std::string& guid : it->second) {
        // |devices_| should be initialized when |ephemeral_devices_| is filled.
        // Because |ephemeral_devices_| is filled by GrantDevicePermission()
        // which is called in HidChooserController::Select(), this method will
        // always be called after device initialization in HidChooserController
        // which always returns after the device list initialization in this
        // class.
        DCHECK(base::Contains(devices_, guid));
        objects.push_back(std::make_unique<ChooserContextBase::Object>(
            requesting_origin, embedding_origin,
            DeviceInfoToValue(*devices_[guid]),
            content_settings::SettingSource::SETTING_SOURCE_USER,
            is_incognito_));
      }
    }
  }

  // TODO(crbug.com/1049825): Include policy-granted objects.

  return objects;
}

std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
HidChooserContext::GetAllGrantedObjects() {
  std::vector<std::unique_ptr<ChooserContextBase::Object>> objects =
      ChooserContextBase::GetAllGrantedObjects();

  for (const auto& map_entry : ephemeral_devices_) {
    const url::Origin& requesting_origin = map_entry.first.first;
    const url::Origin& embedding_origin = map_entry.first.second;

    if (!CanRequestObjectPermission(requesting_origin, embedding_origin))
      continue;

    for (const auto& guid : map_entry.second) {
      DCHECK(base::Contains(devices_, guid));
      objects.push_back(std::make_unique<ChooserContextBase::Object>(
          requesting_origin, embedding_origin,
          DeviceInfoToValue(*devices_[guid]),
          content_settings::SettingSource::SETTING_SOURCE_USER, is_incognito_));
    }
  }

  // TODO(crbug.com/1049825): Include policy-granted objects.

  return objects;
}

void HidChooserContext::RevokeObjectPermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const base::Value& object) {
  const std::string* guid = object.FindStringKey(kHidGuidKey);

  if (!guid) {
    ChooserContextBase::RevokeObjectPermission(requesting_origin,
                                               embedding_origin, object);
    // TODO(crbug.com/964041): Record UMA (WEBHID_PERMISSION_REVOKED).
    return;
  }

  auto it = ephemeral_devices_.find({requesting_origin, embedding_origin});
  if (it != ephemeral_devices_.end()) {
    std::set<std::string>& devices = it->second;

    DCHECK(IsValidObject(object));
    devices.erase(*guid);
    if (devices.empty())
      ephemeral_devices_.erase(it);
    NotifyPermissionRevoked(requesting_origin, embedding_origin);
  }

  // TODO(crbug.com/964041): Record UMA (WEBHID_PERMISSION_REVOKED_EPHEMERAL).
}

void HidChooserContext::GrantDevicePermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const device::mojom::HidDeviceInfo& device) {
  devices_[device.guid] = device.Clone();
  if (CanStorePersistentEntry(device)) {
    GrantObjectPermission(requesting_origin, embedding_origin,
                          DeviceInfoToValue(device));
  } else {
    ephemeral_devices_[std::make_pair(requesting_origin, embedding_origin)]
        .insert(device.guid);
    NotifyPermissionChanged();
  }
}

bool HidChooserContext::HasDevicePermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const device::mojom::HidDeviceInfo& device) {
  if (UsbBlocklist::Get().IsExcluded(
          {device.vendor_id, device.product_id, 0})) {
    return false;
  }

  if (!CanRequestObjectPermission(requesting_origin, embedding_origin))
    return false;

  auto it = ephemeral_devices_.find(
      std::make_pair(requesting_origin, embedding_origin));
  if (it != ephemeral_devices_.end() &&
      base::Contains(it->second, device.guid)) {
    return true;
  }

  std::vector<std::unique_ptr<ChooserContextBase::Object>> object_list =
      GetGrantedObjects(requesting_origin, embedding_origin);
  for (const auto& object : object_list) {
    const base::Value& device_value = object->value;
    DCHECK(IsValidObject(device_value));

    if (device.vendor_id != *device_value.FindIntKey(kHidVendorIdKey) ||
        device.product_id != *device_value.FindIntKey(kHidProductIdKey)) {
      continue;
    }

    const auto* serial_number = device_value.FindStringKey(kHidSerialNumberKey);
    if (serial_number && device.serial_number == *serial_number)
      return true;
  }
  return false;
}

device::mojom::HidManager* HidChooserContext::GetHidManager() {
  EnsureHidManagerConnection();
  return hid_manager_.get();
}

void HidChooserContext::SetHidManagerForTesting(
    mojo::PendingRemote<device::mojom::HidManager> manager) {
  SetUpHidManagerConnection(std::move(manager));
}

base::WeakPtr<HidChooserContext> HidChooserContext::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void HidChooserContext::EnsureHidManagerConnection() {
  if (hid_manager_)
    return;

  mojo::PendingRemote<device::mojom::HidManager> manager;
  content::GetDeviceService().BindHidManager(
      manager.InitWithNewPipeAndPassReceiver());
  SetUpHidManagerConnection(std::move(manager));
}

void HidChooserContext::SetUpHidManagerConnection(
    mojo::PendingRemote<device::mojom::HidManager> manager) {
  hid_manager_.Bind(std::move(manager));
  hid_manager_.set_disconnect_handler(base::BindOnce(
      &HidChooserContext::OnHidManagerConnectionError, base::Unretained(this)));
  // TODO(crbug.com/1082303): Register a HidManagerClient to be notified when
  // devices are disconnected so that ephemeral permissions can be revoked.
}

void HidChooserContext::OnHidManagerConnectionError() {
  devices_.clear();

  std::vector<std::pair<url::Origin, url::Origin>> revoked_origins;
  revoked_origins.reserve(ephemeral_devices_.size());
  for (const auto& map_entry : ephemeral_devices_)
    revoked_origins.push_back(map_entry.first);
  ephemeral_devices_.clear();

  // Notify permission observers that all ephemeral permissions have been
  // revoked.
  for (auto& observer : permission_observer_list_) {
    observer.OnChooserObjectPermissionChanged(guard_content_settings_type_,
                                              data_content_settings_type_);
    for (const auto& origin : revoked_origins)
      observer.OnPermissionRevoked(origin.first, origin.second);
  }
}
