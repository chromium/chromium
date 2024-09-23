// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_pref_updater_impl.h"

#include "base/json/values_util.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_media_sink_util.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace media_router {
AccessCodeCastPrefUpdaterImpl::AccessCodeCastPrefUpdaterImpl(
    PrefService* service)
    : pref_service_(service) {
  DCHECK(pref_service_) << "The pref service does not exist!";
}

void AccessCodeCastPrefUpdaterImpl::UpdateDevicesDict(
    const MediaSinkInternal& sink,
    base::OnceClosure on_updated_callback) {
  ScopedDictPrefUpdate update(pref_service_, prefs::kAccessCodeCastDevices);

  // In order to make sure that the same sink won't be added to the network
  // multiple times, we use ip-based deduping of stored media sinks to
  // ensure that the same sink (possibly with a different older stored named)
  // isn't stored twice in the cast list.
  for (auto existing_sink_ids :
       AccessCodeCastPrefUpdaterImpl::GetMatchingIPEndPoints(
           pref_service_->GetDict(prefs::kAccessCodeCastDevices),
           sink.cast_data().ip_endpoint)) {
    update->Remove(existing_sink_ids);
  }

  update->Set(sink.id(), CreateValueDictFromMediaSinkInternal(sink));
  std::move(on_updated_callback).Run();
}

// This stored preference looks like:
//   "prefs::kAccessCodeCastDeviceAdditionTime": {
//     A string-flavored base::value representing the int64_t number of
//     microseconds since the Windows epoch, using base::TimeToValue().
//     "<sink_id_1>": "1237234734723747234",
//     "<sink_id_2>": "12372347312312347234",
//   }
void AccessCodeCastPrefUpdaterImpl::UpdateDeviceAddedTimeDict(
    const MediaSink::Id sink_id,
    base::OnceClosure on_updated_callback) {
  ScopedDictPrefUpdate update(pref_service_,
                              prefs::kAccessCodeCastDeviceAdditionTime);

  // If the key doesn't exist, or exists but isn't a DictionaryValue, a new
  // DictionaryValue will be created and attached to the path in that location.
  update->Set(sink_id, base::TimeToValue(base::Time::Now()));
  std::move(on_updated_callback).Run();
}

void AccessCodeCastPrefUpdaterImpl::GetDevicesDict(
    base::OnceCallback<void(base::Value::Dict)> get_devices_callback) {
  std::move(get_devices_callback)
      .Run(pref_service_->GetDict(prefs::kAccessCodeCastDevices).Clone());
}

void AccessCodeCastPrefUpdaterImpl::GetDeviceAddedTimeDict(
    base::OnceCallback<void(base::Value::Dict)>
        get_device_added_time_callback) {
  std::move(get_device_added_time_callback)
      .Run(pref_service_->GetDict(prefs::kAccessCodeCastDeviceAdditionTime)
               .Clone());
}

void AccessCodeCastPrefUpdaterImpl::RemoveSinkIdFromDevicesDict(
    const MediaSink::Id sink_id,
    base::OnceClosure on_sink_removed_callback) {
  ScopedDictPrefUpdate update(pref_service_, prefs::kAccessCodeCastDevices);
  update->Remove(sink_id);
  std::move(on_sink_removed_callback).Run();
}

void AccessCodeCastPrefUpdaterImpl::RemoveSinkIdFromDeviceAddedTimeDict(
    const MediaSink::Id sink_id,
    base::OnceClosure on_sink_removed_callback) {
  ScopedDictPrefUpdate update(pref_service_,
                              prefs::kAccessCodeCastDeviceAdditionTime);
  update->Remove(sink_id);
  std::move(on_sink_removed_callback).Run();
}

void AccessCodeCastPrefUpdaterImpl::ClearDevicesDict(
    base::OnceClosure on_cleared_callback) {
  pref_service_->SetDict(prefs::kAccessCodeCastDevices, base::Value::Dict());
  std::move(on_cleared_callback).Run();
}

void AccessCodeCastPrefUpdaterImpl::ClearDeviceAddedTimeDict(
    base::OnceClosure on_cleared_callback) {
  pref_service_->SetDict(prefs::kAccessCodeCastDeviceAdditionTime,
                         base::Value::Dict());
  std::move(on_cleared_callback).Run();
}

void AccessCodeCastPrefUpdaterImpl::UpdateDevicesDictForTesting(
    const MediaSinkInternal& sink) {
  ScopedDictPrefUpdate update(pref_service_, prefs::kAccessCodeCastDevices);

  update->Set(sink.id(), CreateValueDictFromMediaSinkInternal(sink));
}

}  // namespace media_router
