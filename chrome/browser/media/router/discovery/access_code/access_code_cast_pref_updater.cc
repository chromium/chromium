// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_pref_updater.h"

#include "base/json/values_util.h"
#include "base/values.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_media_sink_util.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"

namespace media_router {

using ::prefs::DictionaryValueUpdate;
using ::prefs::ScopedDictionaryPrefUpdate;

AccessCodeCastPrefUpdater::AccessCodeCastPrefUpdater(PrefService* service)
    : pref_service_(service) {
  DCHECK(pref_service_) << "The pref service does not exist!";
}

AccessCodeCastPrefUpdater::~AccessCodeCastPrefUpdater() = default;

// This stored preference looks like:
//   "prefs::kAccessCodeCastDevices": {
//     "<cast1>:1234234": {
//       "sink": {
//         "sink_id": "<cast1>:1234234",
//         "display_name": "Karls Cast Device",
//       },
//       "extra_data": {
//         "capabilities": 4,
//         "port": 666,
//         "ip_address": ""192.0.2.146"",
//       },
//     },
//   }

void AccessCodeCastPrefUpdater::UpdateDevicesDict(
    const MediaSinkInternal& sink) {
  ScopedDictionaryPrefUpdate update(pref_service_,
                                    prefs::kAccessCodeCastDevices);
  std::unique_ptr<DictionaryValueUpdate> devices_pref = update.Get();
  DCHECK(devices_pref) << "The " << prefs::kAccessCodeCastDevices
                       << " pref does not exist.";

  devices_pref->SetDictionary(
      sink.id(), base::DictionaryValue::From(base::Value::ToUniquePtrValue(
                     CreateValueDictFromMediaSinkInternal(sink))));
}

// This stored preference looks like:
//   "prefs::kAccessCodeCastDiscoveredNetworks": {
//     "<network_id_1>": [<sink_id_1>, <sink_id_2>],
//     "<network_id_2>": [<sink_id_4>, <sink_id_6>],
//   }
void AccessCodeCastPrefUpdater::UpdateDiscoveredNetworksDict(
    const MediaSink::Id sink_id,
    const std::string& network_id) {
  ScopedDictionaryPrefUpdate update(pref_service_,
                                    prefs::kAccessCodeCastDiscoveredNetworks);
  std::unique_ptr<DictionaryValueUpdate> discovered_networks_pref =
      update.Get();
  DCHECK(discovered_networks_pref)
      << "The " << prefs::kAccessCodeCastDiscoveredNetworks
      << " pref does not exist.";

  base::ListValue* current_network_list = nullptr;
  bool network_id_exists =
      discovered_networks_pref->GetList(network_id, &current_network_list);

  // There already exists a list with remembered devices on the current
  // network_id. Simply append the sink_id to this network_id.
  if (network_id_exists) {
    // Make sure the sink_id doesn't already exist so we don't duplicate the
    // the entry.
    for (const base::Value& current_sink_id :
         current_network_list->GetListDeprecated()) {
      if (*current_sink_id.GetIfString() == sink_id)
        return;
    }
    current_network_list->Append(sink_id);
  } else {
    // There does not exist any remembered devices on this network yet. Create a
    // new list with the current sink_id and set that list in the dictionary.
    auto sink_ids = std::make_unique<base::Value>(base::Value::Type::LIST);
    sink_ids->Append(sink_id);
    discovered_networks_pref->Set(network_id, std::move(sink_ids));
  }
}

// This stored preference looks like:
//   "prefs::kAccessCodeCastDeviceAdditionTime": {
//     A string-flavored base::value representing the int64_t number of
//     microseconds since the Windows epoch, using base::TimeToValue().
//     "<sink_id_1>": "1237234734723747234",
//     "<sink_id_2>": "12372347312312347234",
//   }
void AccessCodeCastPrefUpdater::UpdateDeviceAdditionTimeDict(
    const MediaSink::Id sink_id) {
  ScopedDictionaryPrefUpdate update(pref_service_,
                                    prefs::kAccessCodeCastDeviceAdditionTime);

  std::unique_ptr<DictionaryValueUpdate> device_time_pref = update.Get();
  DCHECK(device_time_pref) << "The " << prefs::kAccessCodeCastDeviceAdditionTime
                           << " pref does not exist.";

  // If the key doesn't exist, or exists but isn't a DictionaryValue, a new
  // DictionaryValue will be created and attached to the path in that location.
  device_time_pref->SetKey(sink_id, base::TimeToValue(base::Time::Now()));
}

const base::Value* AccessCodeCastPrefUpdater::GetDevicesDict() {
  return pref_service_->GetDictionary(prefs::kAccessCodeCastDevices);
}

const base::Value* AccessCodeCastPrefUpdater::GetDiscoveredNetworksDict() {
  return pref_service_->GetDictionary(prefs::kAccessCodeCastDiscoveredNetworks);
}

const base::Value* AccessCodeCastPrefUpdater::GetDeviceAdditionTimeDict() {
  return pref_service_->GetDictionary(prefs::kAccessCodeCastDeviceAdditionTime);
}

const base::Value::List* AccessCodeCastPrefUpdater::GetSinkIdsByNetworkId(
    const std::string& network_id) {
  auto* network_dict = GetDiscoveredNetworksDict();
  if (!network_dict)
    return nullptr;

  // If found, it returns a pointer to the element. Otherwise it returns
  // nullptr.
  auto* network_list = network_dict->FindKey(network_id);

  if (!network_list)
    return nullptr;
  return network_list->GetIfList();
}

const base::Value* AccessCodeCastPrefUpdater::GetMediaSinkInternalValueBySinkId(
    const MediaSink::Id sink_id) {
  auto* device_dict = GetDevicesDict();
  if (!device_dict)
    return nullptr;

  // If found, it returns a pointer to the element. Otherwise it returns
  // nullptr.
  auto* device_value = device_dict->FindKey(sink_id);

  if (!device_value)
    return nullptr;
  return device_value;
}

void AccessCodeCastPrefUpdater::RemoveSinkIdFromDevicesDict(
    const MediaSink::Id sink_id) {
  ScopedDictionaryPrefUpdate update(pref_service_,
                                    prefs::kAccessCodeCastDevices);
  std::unique_ptr<DictionaryValueUpdate> devices_pref = update.Get();
  DCHECK(devices_pref) << "The " << prefs::kAccessCodeCastDevices
                       << " pref does not exist.";
  devices_pref->Remove(sink_id);
}

void AccessCodeCastPrefUpdater::RemoveSinkIdFromDiscoveredNetworksDict(
    const MediaSink::Id sink_id) {
  ScopedDictionaryPrefUpdate update(pref_service_,
                                    prefs::kAccessCodeCastDiscoveredNetworks);
  std::unique_ptr<DictionaryValueUpdate> discovered_networks_pref =
      update.Get();
  DCHECK(discovered_networks_pref)
      << "The " << prefs::kAccessCodeCastDiscoveredNetworks
      << " pref does not exist.";

  const base::Value::Dict network_dict =
      GetDiscoveredNetworksDict()->GetDict().Clone();

  // Iterate through network id's
  for (auto key_value_pair : network_dict) {
    const auto network_id = key_value_pair.first;

    // We need to clone here because the GetList() method returns a const value
    // and we might need to modify it later on.
    base::Value::List network_list = key_value_pair.second.GetList().Clone();

    // Check to see if the media sink was erased from the list. If there is no
    // value in the list simply continue in the iteration.
    if (!network_list.EraseValue(base::Value(sink_id)))
      continue;

    // If the list is now empty, remove the key from the dictionary
    // entirely and continue from this iteration.

    if (network_list.empty()) {
      bool removal_result = discovered_networks_pref->Remove(network_id);
      DCHECK(removal_result)
          << "The network_id list value exists but the key does not.";
      continue;
    }
    discovered_networks_pref->Set(
        network_id,
        base::Value::ToUniquePtrValue(base::Value(std::move(network_list))));
  }
}

void AccessCodeCastPrefUpdater::RemoveSinkIdFromDeviceAdditionTimeDict(
    const MediaSink::Id sink_id) {
  ScopedDictionaryPrefUpdate update(pref_service_,
                                    prefs::kAccessCodeCastDeviceAdditionTime);

  std::unique_ptr<DictionaryValueUpdate> device_time_pref = update.Get();
  DCHECK(device_time_pref) << "The " << prefs::kAccessCodeCastDeviceAdditionTime
                           << " pref does not exist.";
  device_time_pref->Remove(sink_id);
}

base::WeakPtr<AccessCodeCastPrefUpdater>
AccessCodeCastPrefUpdater::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
}  // namespace media_router
