// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/serial_chooser_context.h"

#include <utility>

#include "base/base64.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/system_connector.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

constexpr char kPortNameKey[] = "name";
constexpr char kTokenKey[] = "token";

std::string EncodeToken(const base::UnguessableToken& token) {
  const uint64_t data[2] = {token.GetHighForSerialization(),
                            token.GetLowForSerialization()};
  std::string buffer;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(&data[0]), sizeof(data)),
      &buffer);
  return buffer;
}

base::UnguessableToken DecodeToken(base::StringPiece input) {
  std::string buffer;
  if (!base::Base64Decode(input, &buffer) ||
      buffer.length() != sizeof(uint64_t) * 2) {
    return base::UnguessableToken();
  }

  const uint64_t* data = reinterpret_cast<const uint64_t*>(buffer.data());
  return base::UnguessableToken::Deserialize(data[0], data[1]);
}

base::Value PortInfoToValue(const device::mojom::SerialPortInfo& port) {
  base::Value value(base::Value::Type::DICTIONARY);
  if (port.display_name)
    value.SetStringKey(kPortNameKey, *port.display_name);
  else
    value.SetStringKey(kPortNameKey, port.path.LossyDisplayName());
  value.SetStringKey(kTokenKey, EncodeToken(port.token));
  return value;
}

}  // namespace

SerialChooserContext::SerialChooserContext(Profile* profile)
    : ChooserContextBase(profile,
                         ContentSettingsType::SERIAL_GUARD,
                         ContentSettingsType::SERIAL_CHOOSER_DATA),
      is_incognito_(profile->IsOffTheRecord()) {}

SerialChooserContext::~SerialChooserContext() = default;

bool SerialChooserContext::IsValidObject(const base::Value& object) {
  const std::string* token = object.FindStringKey(kTokenKey);
  return object.is_dict() && object.DictSize() == 2 &&
         object.FindStringKey(kPortNameKey) && token && DecodeToken(*token);
}

// static
std::string SerialChooserContext::GetObjectName(const base::Value& object) {
  const std::string* name = object.FindStringKey(kPortNameKey);
  DCHECK(name);
  return *name;
}

std::vector<std::unique_ptr<ChooserContextBase::Object>>
SerialChooserContext::GetGrantedObjects(const url::Origin& requesting_origin,
                                        const url::Origin& embedding_origin) {
  if (!CanRequestObjectPermission(requesting_origin, embedding_origin))
    return {};

  auto origin_it = ephemeral_ports_.find(
      std::make_pair(requesting_origin, embedding_origin));
  if (origin_it == ephemeral_ports_.end())
    return {};
  const std::set<base::UnguessableToken> ports = origin_it->second;

  std::vector<std::unique_ptr<Object>> objects;
  for (const auto& token : ports) {
    auto it = port_info_.find(token);
    if (it == port_info_.end())
      continue;

    objects.push_back(std::make_unique<Object>(
        requesting_origin, embedding_origin, it->second.Clone(),
        content_settings::SettingSource::SETTING_SOURCE_USER, is_incognito_));
  }

  return objects;
}

std::vector<std::unique_ptr<ChooserContextBase::Object>>
SerialChooserContext::GetAllGrantedObjects() {
  std::vector<std::unique_ptr<Object>> objects;
  for (const auto& map_entry : ephemeral_ports_) {
    const url::Origin& requesting_origin = map_entry.first.first;
    const url::Origin& embedding_origin = map_entry.first.second;

    if (!CanRequestObjectPermission(requesting_origin, embedding_origin))
      continue;

    for (const auto& token : map_entry.second) {
      auto it = port_info_.find(token);
      if (it == port_info_.end())
        continue;

      objects.push_back(std::make_unique<Object>(
          requesting_origin, embedding_origin, it->second.Clone(),
          content_settings::SettingSource::SETTING_SOURCE_USER, is_incognito_));
    }
  }

  return objects;
}

void SerialChooserContext::RevokeObjectPermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const base::Value& object) {
  auto origin_it = ephemeral_ports_.find(
      std::make_pair(requesting_origin, embedding_origin));
  if (origin_it == ephemeral_ports_.end())
    return;
  std::set<base::UnguessableToken>& ports = origin_it->second;

  DCHECK(IsValidObject(object));
  ports.erase(DecodeToken(*object.FindStringKey(kTokenKey)));
  NotifyPermissionRevoked(requesting_origin, embedding_origin);
}

void SerialChooserContext::GrantPortPermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const device::mojom::SerialPortInfo& port) {
  // TODO(crbug.com/908836): If |port| can be remembered persistently call into
  // ChooserContextBase to store it in user preferences.
  ephemeral_ports_[std::make_pair(requesting_origin, embedding_origin)].insert(
      port.token);
  port_info_[port.token] = PortInfoToValue(port);
  NotifyPermissionChanged();
}

bool SerialChooserContext::HasPortPermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const device::mojom::SerialPortInfo& port) {
  if (!CanRequestObjectPermission(requesting_origin, embedding_origin)) {
    return false;
  }

  auto origin_it = ephemeral_ports_.find(
      std::make_pair(requesting_origin, embedding_origin));
  if (origin_it == ephemeral_ports_.end())
    return false;
  const std::set<base::UnguessableToken> ports = origin_it->second;

  // TODO(crbug.com/908836): Call into ChooserContextBase to check persistent
  // permissions.
  auto port_it = ports.find(port.token);
  return port_it != ports.end();
}

device::mojom::SerialPortManager* SerialChooserContext::GetPortManager() {
  EnsurePortManagerConnection();
  return port_manager_.get();
}

void SerialChooserContext::SetPortManagerForTesting(
    mojo::PendingRemote<device::mojom::SerialPortManager> manager) {
  SetUpPortManagerConnection(std::move(manager));
}

base::WeakPtr<SerialChooserContext> SerialChooserContext::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void SerialChooserContext::EnsurePortManagerConnection() {
  if (port_manager_)
    return;

  mojo::PendingRemote<device::mojom::SerialPortManager> manager;
  content::GetSystemConnector()->Connect(
      device::mojom::kServiceName, manager.InitWithNewPipeAndPassReceiver());
  SetUpPortManagerConnection(std::move(manager));
}

void SerialChooserContext::SetUpPortManagerConnection(
    mojo::PendingRemote<device::mojom::SerialPortManager> manager) {
  port_manager_.Bind(std::move(manager));
  port_manager_.set_disconnect_handler(
      base::BindOnce(&SerialChooserContext::OnPortManagerConnectionError,
                     base::Unretained(this)));
}

void SerialChooserContext::OnPortManagerConnectionError() {
  port_info_.clear();

  std::vector<std::pair<url::Origin, url::Origin>> revoked_origins;
  revoked_origins.reserve(ephemeral_ports_.size());
  for (const auto& map_entry : ephemeral_ports_)
    revoked_origins.push_back(map_entry.first);
  ephemeral_ports_.clear();

  // Notify permission observers that all ephemeral permissions have been
  // revoked.
  for (auto& observer : permission_observer_list_) {
    observer.OnChooserObjectPermissionChanged(guard_content_settings_type_,
                                              data_content_settings_type_);
    for (const auto& origin : revoked_origins)
      observer.OnPermissionRevoked(origin.first, origin.second);
  }
}
