// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_pref_updater.h"

#include "base/json/values_util.h"
#include "base/values.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_media_sink_util.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"

namespace media_router {

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
  ScopedDictPrefUpdate update(pref_service_, prefs::kAccessCodeCastDevices);

  // In order to make sure that the same sink won't be added to the network
  // multiple times, we use ip-based deduping of stored media sinks to
  // ensure that the same sink (possibly with a different older stored named)
  // isn't stored twice in the cast list.
  for (auto existing_sink_ids :
       GetMatchingIPEndPoints(sink.cast_data().ip_endpoint)) {
    update->Remove(existing_sink_ids);
  }

  update->Set(sink.id(), CreateValueDictFromMediaSinkInternal(sink));
}

// This stored preference looks like:
//   "prefs::kAccessCodeCastDeviceAdditionTime": {
//     A string-flavored base::value representing the int64_t number of
//     microseconds since the Windows epoch, using base::TimeToValue().
//     "<sink_id_1>": "1237234734723747234",
//     "<sink_id_2>": "12372347312312347234",
//   }
void AccessCodeCastPrefUpdater::UpdateDeviceAddedTimeDict(
    const MediaSink::Id sink_id) {
  ScopedDictPrefUpdate update(pref_service_,
                              prefs::kAccessCodeCastDeviceAdditionTime);

  // If the key doesn't exist, or exists but isn't a DictionaryValue, a new
  // DictionaryValue will be created and attached to the path in that location.
  update->Set(sink_id, base::TimeToValue(base::Time::Now()));
}

const base::Value::Dict& AccessCodeCastPrefUpdater::GetDevicesDict() {
  return pref_service_->GetDict(prefs::kAccessCodeCastDevices);
}

const base::Value::Dict& AccessCodeCastPrefUpdater::GetDeviceAddedTimeDict() {
  return pref_service_->GetDict(prefs::kAccessCodeCastDeviceAdditionTime);
}

const base::Value::List AccessCodeCastPrefUpdater::GetSinkIdsFromDevicesDict() {
  auto sink_ids = base::Value::List();

  const auto& devices_dict = GetDevicesDict();
  for (auto sink_id_keypair : devices_dict) {
    sink_ids.Append(sink_id_keypair.first);
  }
  return sink_ids;
}

const base::Value* AccessCodeCastPrefUpdater::GetMediaSinkInternalValueBySinkId(
    const MediaSink::Id sink_id) {
  const auto& device_dict = GetDevicesDict();

  // If found, it returns a pointer to the element. Otherwise it returns
  // nullptr.
  const auto* device_value = device_dict.Find(sink_id);

  if (!device_value) {
    return nullptr;
  }
  return device_value;
}

absl::optional<base::Time> AccessCodeCastPrefUpdater::GetDeviceAddedTime(
    const MediaSink::Id sink_id) {
  const auto& device_Added_dict = GetDeviceAddedTimeDict();

  // If found, it returns a pointer to the element. Otherwise it returns
  // nullptr.
  auto* device_Added_value = device_Added_dict.Find(sink_id);

  if (!device_Added_value) {
    return absl::nullopt;
  }
  return base::ValueToTime(device_Added_value);
}

void AccessCodeCastPrefUpdater::RemoveSinkIdFromDevicesDict(
    const MediaSink::Id sink_id) {
  ScopedDictPrefUpdate update(pref_service_, prefs::kAccessCodeCastDevices);
  update->Remove(sink_id);
}

void AccessCodeCastPrefUpdater::RemoveSinkIdFromDeviceAddedTimeDict(
    const MediaSink::Id sink_id) {
  ScopedDictPrefUpdate update(pref_service_,
                              prefs::kAccessCodeCastDeviceAdditionTime);
  update->Remove(sink_id);
}

void AccessCodeCastPrefUpdater::ClearDevicesDict() {
  pref_service_->SetDict(prefs::kAccessCodeCastDevices, base::Value::Dict());
}

void AccessCodeCastPrefUpdater::ClearDeviceAddedTimeDict() {
  pref_service_->SetDict(prefs::kAccessCodeCastDeviceAdditionTime,
                         base::Value::Dict());
}

std::vector<MediaSink::Id> AccessCodeCastPrefUpdater::GetMatchingIPEndPoints(
    net::IPEndPoint ip_endpoint) {
  std::vector<MediaSink::Id> duplicate_sinks;

  const auto& devices_dict = GetDevicesDict();

  // Iterate through device dictionaries, fetch ip_endpoints, and then check if
  // these ip_endpoint match the ip_endpoint argument.
  for (auto sink_id_keypair : devices_dict) {
    const auto* dict_value = sink_id_keypair.second.GetIfDict();
    if (!dict_value) {
      break;
    }

    auto fetched_ip_endpoint = GetIPEndPointFromValueDict(*dict_value);
    if (!fetched_ip_endpoint.has_value()) {
      break;
    }

    if (ip_endpoint == fetched_ip_endpoint) {
      duplicate_sinks.push_back(sink_id_keypair.first);
    }
  }
  return duplicate_sinks;
}

void AccessCodeCastPrefUpdater::UpdateDevicesDictForTest(
    const MediaSinkInternal& sink) {
  ScopedDictPrefUpdate update(pref_service_, prefs::kAccessCodeCastDevices);

  update->Set(sink.id(), CreateValueDictFromMediaSinkInternal(sink));
}

base::WeakPtr<AccessCodeCastPrefUpdater>
AccessCodeCastPrefUpdater::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
}  // namespace media_router
