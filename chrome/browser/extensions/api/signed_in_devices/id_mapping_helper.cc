// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/signed_in_devices/id_mapping_helper.h"

#include <memory>

#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/signed_in_devices/signed_in_devices_api.h"
#include "chrome/browser/profiles/profile.h"
#include "components/crx_file/id_util.h"
#include "components/sync_device_info/device_info.h"

using base::DictionaryValue;
using base::Value;
using syncer::DeviceInfo;

namespace extensions {

std::string GetPublicIdFromGUID(
    const base::DictionaryValue& id_mapping,
    const std::string& guid) {
  for (base::DictionaryValue::Iterator it(id_mapping);
       !it.IsAtEnd();
       it.Advance()) {
    const base::Value& value = it.value();
    std::string guid_in_value;
    if (!value.GetAsString(&guid_in_value)) {
      LOG(ERROR) << "Badly formatted dictionary";
      continue;
    }
    if (guid_in_value == guid) {
      return it.key();
    }
  }

  return std::string();
}

std::string GetGUIDFromPublicId(
    const base::DictionaryValue& id_mapping,
    const std::string& id) {
  std::string guid;
  id_mapping.GetString(id, &guid);
  return guid;
}

// Finds out a random unused id. First finds a random id.
// If the id is in use, increments the id until it finds an unused id.
std::string GetRandomId(
  const base::DictionaryValue& mapping,
  int device_count) {
  // Set the max value for rand to be twice the device count.
  int max = device_count * 2;
  int rand_value = base::RandInt(0, max);
  std::string string_value;
  const base::Value *out_value;

  do {
    string_value = base::NumberToString(rand_value);
    rand_value++;
  } while (mapping.Get(string_value, &out_value));

  return string_value;
}

void CreateMappingForUnmappedDevices(
    const std::vector<std::unique_ptr<DeviceInfo>>& device_info,
    base::DictionaryValue* value) {
  for (const std::unique_ptr<DeviceInfo>& device : device_info) {
    std::string local_id = GetPublicIdFromGUID(*value,
                                               device->guid());

    // If the device does not have a local id, set one.
    if (local_id.empty()) {
      local_id = GetRandomId(*value, device_info.size());
      value->SetString(local_id, device->guid());
    }
    device->set_public_id(local_id);
  }
}

std::unique_ptr<DeviceInfo> GetDeviceInfoForClientId(
    const std::string& client_id,
    const std::string& extension_id,
    Profile* profile) {
  DCHECK(crx_file::id_util::IdIsValid(extension_id)) << extension_id
                                                     << " is not valid";
  std::vector<std::unique_ptr<DeviceInfo>> devices =
      GetAllSignedInDevices(extension_id, profile);
  for (auto& iter : devices) {
    if (iter->guid() == client_id) {
      std::unique_ptr<DeviceInfo> device = std::move(iter);
      return device;
    }
  }
  return std::unique_ptr<DeviceInfo>();
}

}  // namespace  extensions
