// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_chooser_context.h"

#include <utility>

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/system_connector.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

constexpr char kHidDeviceNameKey[] = "name";
constexpr char kHidGuidKey[] = "guid";

base::Value DeviceInfoToValue(const device::mojom::HidDeviceInfo& device) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetStringKey(kHidDeviceNameKey, device.product_name);
  // The GUID is a temporary ID created on connection that remains valid until
  // the device is disconnected. Ephemeral permissions are keyed by this ID and
  // must be granted again each time the device is connected.
  // TODO(crbug.com/958918): Extract a persistent identifier to allow device
  // permissions to be retained after the device is disconnected.
  value.SetStringKey(kHidGuidKey, device.guid);
  return value;
}

}  // namespace

HidChooserContext::HidChooserContext(Profile* profile)
    : ChooserContextBase(profile,
                         ContentSettingsType::HID_GUARD,
                         ContentSettingsType::HID_CHOOSER_DATA),
      is_incognito_(profile->IsOffTheRecord()) {}

HidChooserContext::~HidChooserContext() = default;

// static
std::string HidChooserContext::GetObjectName(const base::Value& object) {
  const std::string* name = object.FindStringKey(kHidDeviceNameKey);
  DCHECK(name);
  return *name;
}

bool HidChooserContext::IsValidObject(const base::Value& object) {
  if (!object.is_dict() || object.DictSize() != 2 ||
      !object.FindStringKey(kHidDeviceNameKey)) {
    return false;
  }

  const std::string* guid = object.FindStringKey(kHidGuidKey);
  return guid && !guid->empty();
}

std::vector<std::unique_ptr<ChooserContextBase::Object>>
HidChooserContext::GetGrantedObjects(const url::Origin& requesting_origin,
                                     const url::Origin& embedding_origin) {
  // TODO(crbug.com/958918): Include devices with persistent permissions in the
  // returned list.
  if (!CanRequestObjectPermission(requesting_origin, embedding_origin))
    return {};

  auto origin_it = ephemeral_devices_.find(
      std::make_pair(requesting_origin, embedding_origin));
  if (origin_it == ephemeral_devices_.end())
    return {};

  const std::set<std::string> devices = origin_it->second;

  std::vector<std::unique_ptr<Object>> objects;
  for (const auto& guid : devices) {
    auto it = device_info_.find(guid);
    if (it == device_info_.end())
      continue;

    objects.push_back(std::make_unique<Object>(
        requesting_origin, embedding_origin, it->second.Clone(),
        content_settings::SettingSource::SETTING_SOURCE_USER, is_incognito_));
  }

  return objects;
}

std::vector<std::unique_ptr<ChooserContextBase::Object>>
HidChooserContext::GetAllGrantedObjects() {
  // TODO(crbug.com/958918): Include devices with persistent permissions in the
  // returned list.
  std::vector<std::unique_ptr<Object>> objects;
  for (const auto& map_entry : ephemeral_devices_) {
    const url::Origin& requesting_origin = map_entry.first.first;
    const url::Origin& embedding_origin = map_entry.first.second;

    if (!CanRequestObjectPermission(requesting_origin, embedding_origin))
      continue;

    for (const auto& guid : map_entry.second) {
      auto it = device_info_.find(guid);
      if (it == device_info_.end())
        continue;

      objects.push_back(std::make_unique<Object>(
          requesting_origin, embedding_origin, it->second.Clone(),
          content_settings::SettingSource::SETTING_SOURCE_USER, is_incognito_));
    }
  }

  return objects;
}

void HidChooserContext::RevokeObjectPermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const base::Value& object) {
  // TODO(crbug.com/958918): Revoke persistent permissions if the device has a
  // persistent ID.
  auto origin_it = ephemeral_devices_.find(
      std::make_pair(requesting_origin, embedding_origin));
  if (origin_it == ephemeral_devices_.end())
    return;

  std::set<std::string>& devices = origin_it->second;

  DCHECK(IsValidObject(object));
  devices.erase(*object.FindStringKey(kHidGuidKey));
  NotifyPermissionRevoked(requesting_origin, embedding_origin);
}

void HidChooserContext::GrantDevicePermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const device::mojom::HidDeviceInfo& device) {
  // TODO(crbug.com/958918): Grant persistent permissions for eligible devices.
  ephemeral_devices_[std::make_pair(requesting_origin, embedding_origin)]
      .insert(device.guid);
  device_info_[device.guid] = DeviceInfoToValue(device);
  NotifyPermissionChanged();
}

bool HidChooserContext::HasDevicePermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const device::mojom::HidDeviceInfo& device) {
  // TODO(crbug.com/958918): Also check if a persistent permission was granted
  // for |device|.
  if (!CanRequestObjectPermission(requesting_origin, embedding_origin)) {
    return false;
  }

  auto origin_it = ephemeral_devices_.find(
      std::make_pair(requesting_origin, embedding_origin));
  if (origin_it == ephemeral_devices_.end())
    return false;

  const std::set<std::string> devices = origin_it->second;

  auto device_it = devices.find(device.guid);
  return device_it != devices.end();
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
  content::GetSystemConnector()->Connect(
      device::mojom::kServiceName, manager.InitWithNewPipeAndPassReceiver());
  SetUpHidManagerConnection(std::move(manager));
}

void HidChooserContext::SetUpHidManagerConnection(
    mojo::PendingRemote<device::mojom::HidManager> manager) {
  hid_manager_.Bind(std::move(manager));
  hid_manager_.set_disconnect_handler(base::BindOnce(
      &HidChooserContext::OnHidManagerConnectionError, base::Unretained(this)));
  // TODO(mattreynolds): Register a HidManagerClient to be notified when devices
  // are disconnected so that ephemeral permissions can be revoked.
}

void HidChooserContext::OnHidManagerConnectionError() {
  device_info_.clear();

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
