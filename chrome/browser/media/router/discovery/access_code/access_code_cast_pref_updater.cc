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

  // TODO(b/231748126): Add IP based-deduping for storing Access Code Cast Sinks
  devices_pref->SetDictionary(
      sink.id(), base::DictionaryValue::From(base::Value::ToUniquePtrValue(
                     CreateValueDictFromMediaSinkInternal(sink))));
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
  ScopedDictionaryPrefUpdate update(pref_service_,
                                    prefs::kAccessCodeCastDeviceAdditionTime);

  std::unique_ptr<DictionaryValueUpdate> device_time_pref = update.Get();
  DCHECK(device_time_pref) << "The " << prefs::kAccessCodeCastDeviceAdditionTime
                           << " pref does not exist.";

  // If the key doesn't exist, or exists but isn't a DictionaryValue, a new
  // DictionaryValue will be created and attached to the path in that location.
  device_time_pref->SetKey(sink_id, base::TimeToValue(base::Time::Now()));
}

const base::Value::Dict& AccessCodeCastPrefUpdater::GetDevicesDict() {
  return pref_service_->GetValueDict(prefs::kAccessCodeCastDevices);
}

const base::Value* AccessCodeCastPrefUpdater::GetDeviceAddedTimeDict() {
  return pref_service_->GetDictionary(prefs::kAccessCodeCastDeviceAdditionTime);
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

  if (!device_value)
    return nullptr;
  return device_value;
}

absl::optional<base::Time> AccessCodeCastPrefUpdater::GetDeviceAddedTime(
    const MediaSink::Id sink_id) {
  auto* device_Added_dict = GetDeviceAddedTimeDict();
  if (!device_Added_dict)
    return absl::nullopt;

  // If found, it returns a pointer to the element. Otherwise it returns
  // nullptr.
  auto* device_Added_value = device_Added_dict->FindKey(sink_id);

  if (!device_Added_value)
    return absl::nullopt;
  return base::ValueToTime(device_Added_value);
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

void AccessCodeCastPrefUpdater::RemoveSinkIdFromDeviceAddedTimeDict(
    const MediaSink::Id sink_id) {
  ScopedDictionaryPrefUpdate update(pref_service_,
                                    prefs::kAccessCodeCastDeviceAdditionTime);

  std::unique_ptr<DictionaryValueUpdate> device_time_pref = update.Get();
  DCHECK(device_time_pref) << "The " << prefs::kAccessCodeCastDeviceAdditionTime
                           << " pref does not exist.";
  device_time_pref->Remove(sink_id);
}

void AccessCodeCastPrefUpdater::ClearDevicesDict() {
  ScopedDictionaryPrefUpdate update(pref_service_,
                                    prefs::kAccessCodeCastDevices);
  std::unique_ptr<DictionaryValueUpdate> devices_pref = update.Get();
  DCHECK(devices_pref) << "The " << prefs::kAccessCodeCastDevices
                       << " pref does not exist.";
  devices_pref->Clear();
}

void AccessCodeCastPrefUpdater::ClearDeviceAddedTimeDict() {
  ScopedDictionaryPrefUpdate update(pref_service_,
                                    prefs::kAccessCodeCastDeviceAdditionTime);

  std::unique_ptr<DictionaryValueUpdate> device_time_pref = update.Get();
  DCHECK(device_time_pref) << "The " << prefs::kAccessCodeCastDeviceAdditionTime
                           << " pref does not exist.";
  device_time_pref->Clear();
}

base::WeakPtr<AccessCodeCastPrefUpdater>
AccessCodeCastPrefUpdater::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
}  // namespace media_router
