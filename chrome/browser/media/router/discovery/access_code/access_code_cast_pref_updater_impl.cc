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
void AccessCodeCastPrefUpdaterImpl::UpdateDeviceAddedTimeDict(
    const MediaSink::Id sink_id) {
  ScopedDictPrefUpdate update(pref_service_,
                              prefs::kAccessCodeCastDeviceAdditionTime);

  // If the key doesn't exist, or exists but isn't a DictionaryValue, a new
  // DictionaryValue will be created and attached to the path in that location.
  update->Set(sink_id, base::TimeToValue(base::Time::Now()));
}

const base::Value::Dict& AccessCodeCastPrefUpdaterImpl::GetDevicesDict() {
  return pref_service_->GetDict(prefs::kAccessCodeCastDevices);
}

const base::Value::Dict&
AccessCodeCastPrefUpdaterImpl::GetDeviceAddedTimeDict() {
  return pref_service_->GetDict(prefs::kAccessCodeCastDeviceAdditionTime);
}

void AccessCodeCastPrefUpdaterImpl::RemoveSinkIdFromDevicesDict(
    const MediaSink::Id sink_id) {
  ScopedDictPrefUpdate update(pref_service_, prefs::kAccessCodeCastDevices);
  update->Remove(sink_id);
}

void AccessCodeCastPrefUpdaterImpl::RemoveSinkIdFromDeviceAddedTimeDict(
    const MediaSink::Id sink_id) {
  ScopedDictPrefUpdate update(pref_service_,
                              prefs::kAccessCodeCastDeviceAdditionTime);
  update->Remove(sink_id);
}

void AccessCodeCastPrefUpdaterImpl::ClearDevicesDict() {
  pref_service_->SetDict(prefs::kAccessCodeCastDevices, base::Value::Dict());
}

void AccessCodeCastPrefUpdaterImpl::ClearDeviceAddedTimeDict() {
  pref_service_->SetDict(prefs::kAccessCodeCastDeviceAdditionTime,
                         base::Value::Dict());
}

void AccessCodeCastPrefUpdaterImpl::UpdateDevicesDictForTest(
    const MediaSinkInternal& sink) {
  ScopedDictPrefUpdate update(pref_service_, prefs::kAccessCodeCastDevices);

  update->Set(sink.id(), CreateValueDictFromMediaSinkInternal(sink));
}

}  // namespace media_router
