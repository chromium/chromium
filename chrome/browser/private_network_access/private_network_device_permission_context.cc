// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_network_access/private_network_device_permission_context.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace {

constexpr char kDeviceNameKey[] = "device-name";
constexpr char kDeviceIdKey[] = "device-id";
constexpr char kIPAddressKey[] = "ip-address";

}  // namespace

PrivateNetworkDevicePermissionContext::PrivateNetworkDevicePermissionContext(
    Profile* profile)
    : ObjectPermissionContextBase(
          ContentSettingsType::PRIVATE_NETWORK_GUARD,
          ContentSettingsType::PRIVATE_NETWORK_CHOOSER_DATA,
          HostContentSettingsMapFactory::GetForProfile(profile)) {}

PrivateNetworkDevicePermissionContext::
    ~PrivateNetworkDevicePermissionContext() = default;

std::string PrivateNetworkDevicePermissionContext::GetKeyForObject(
    const base::Value::Dict& object) {
  if (!IsValidObject(object)) {
    return std::string();
  }
  if (const std::string* device_id = object.FindString(kDeviceIdKey)) {
    return base::StrCat({"id:", *device_id});
  }
  return base::StrCat({"ip:", *(object.FindString(kIPAddressKey))});
}

std::u16string PrivateNetworkDevicePermissionContext::GetObjectDisplayName(
    const base::Value::Dict& object) {
  const std::string* name = object.FindString(kDeviceNameKey);
  DCHECK(name);
  if (!name->empty()) {
    return base::UTF8ToUTF16(*name);
  }

  const std::string* ip_address = object.FindString(kIPAddressKey);
  DCHECK(ip_address);
  return base::UTF8ToUTF16(*ip_address);
}

bool PrivateNetworkDevicePermissionContext::IsValidObject(
    const base::Value::Dict& object) {
  return object.size() == 3 && object.FindString(kDeviceNameKey) &&
         object.FindString(kDeviceIdKey) && object.FindString(kIPAddressKey);
}

void PrivateNetworkDevicePermissionContext::GrantDevicePermission(
    const url::Origin& origin,
    const blink::mojom::PrivateNetworkDevice& device,
    bool is_device_valid) {
  // Store ephemeral permission with IP address.
  if (!is_device_valid) {
    ephemeral_devices_[origin].insert(device.ip_address);
    base::UmaHistogramEnumeration(
        kUserAcceptedPrivateNetworkDeviceHistogramName,
        NewAcceptedDeviceType::kEphemeralDevice);
    return;
  }
  base::UmaHistogramEnumeration(kUserAcceptedPrivateNetworkDeviceHistogramName,
                                NewAcceptedDeviceType::kValidDevice);
  GrantObjectPermission(origin, DeviceInfoToValue(device));
}

bool PrivateNetworkDevicePermissionContext::HasDevicePermission(
    const url::Origin& origin,
    const blink::mojom::PrivateNetworkDevice& device,
    bool is_device_valid) {
  if (is_device_valid) {
    std::vector<std::unique_ptr<Object>> object_list =
        GetGrantedObjects(origin);
    for (const auto& object : object_list) {
      const base::Value::Dict& value = object->value;
      DCHECK(IsValidObject(value));
      DCHECK(device.id.has_value());
      if (*value.FindString(kDeviceIdKey) == device.id.value()) {
        base::UmaHistogramEnumeration(
            kPrivateNetworkDeviceValidityHistogramName,
            PrivateNetworkDeviceValidity::kExistingDevice);
        return true;
      }
    }
  } else {
    // If there's no valid id and name, then look up for ephemeral permission
    // based on IP address.
    auto it = ephemeral_devices_.find(origin);
    if (it != ephemeral_devices_.end()) {
      std::set<net::IPAddress> device_set = it->second;
      auto ip_address = device_set.find(device.ip_address);
      if (ip_address != device_set.end()) {
        return true;
      }
    }
  }

  return false;
}

void PrivateNetworkDevicePermissionContext::Shutdown() {
  FlushScheduledSaveSettingsCalls();
  permissions::ObjectPermissionContextBase::Shutdown();
}

base::WeakPtr<PrivateNetworkDevicePermissionContext>
PrivateNetworkDevicePermissionContext::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// static
base::Value::Dict PrivateNetworkDevicePermissionContext::DeviceInfoToValue(
    const blink::mojom::PrivateNetworkDevice& device) {
  base::Value::Dict device_value;
  device_value.Set(kDeviceNameKey, device.name.value());
  device_value.Set(kDeviceIdKey, device.id.value());
  device_value.Set(kIPAddressKey, device.ip_address.ToString());
  return device_value;
}
