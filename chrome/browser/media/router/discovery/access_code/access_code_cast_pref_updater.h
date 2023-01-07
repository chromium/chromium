// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/media_sink.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

class PrefService;

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_PREF_UPDATER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_PREF_UPDATER_H_

namespace media_router {

// Pref updater for AccessCodeCasting.
class AccessCodeCastPrefUpdater {
 public:
  explicit AccessCodeCastPrefUpdater(PrefService* service);

  AccessCodeCastPrefUpdater(const AccessCodeCastPrefUpdater&) = delete;
  AccessCodeCastPrefUpdater& operator=(const AccessCodeCastPrefUpdater&) =
      delete;

  virtual ~AccessCodeCastPrefUpdater();

  // Sets the key for the given |sink| id with the actual |sink| itself. This
  // function will overwrite a sink id if it already exists.
  void UpdateDevicesDict(const MediaSinkInternal& sink);

  // Sets the key for the |sink_id| with the time it is added. This is
  // calculated at the time of the functions calling. If the |sink_id| already
  // exist, then update the value of that |sink_id| with a new time.
  void UpdateDeviceAddedTimeDict(const MediaSink::Id sink_id);

  // Returns a the device dictionary from the pref service.
  const base::Value::Dict& GetDevicesDict();

  // Returns a nullptr if the device Added dictionary does not exist in the
  // pref service for some reason.
  const base::Value::Dict& GetDeviceAddedTimeDict();

  // Gets a list of all sink ids currently stored in the pref service.
  const base::Value::List GetSinkIdsFromDevicesDict();

  // If found, it returns a pointer to the element. Otherwise it returns
  // nullptr.
  const base::Value* GetMediaSinkInternalValueBySinkId(
      const MediaSink::Id sink_id);

  // If found and a valid time value, returns the time of Addeds.
  absl::optional<base::Time> GetDeviceAddedTime(const MediaSink::Id sink_id);

  // Removes the given |sink_id| from all instances in the devices dictionary
  // stored in the pref service. Nothing occurs if the |sink_id| was not there
  // in the first place.
  void RemoveSinkIdFromDevicesDict(const MediaSink::Id sink_id);

  // Removes the given |sink_id| from all instances in the device Added
  // dictionary stored in the pref service. Nothing occurs if the |sink_id| was
  // not there in the first place.
  void RemoveSinkIdFromDeviceAddedTimeDict(const MediaSink::Id sink_id);

  void ClearDevicesDict();
  void ClearDeviceAddedTimeDict();

  base::WeakPtr<AccessCodeCastPrefUpdater> GetWeakPtr();

 private:
  raw_ptr<PrefService> pref_service_;

  base::WeakPtrFactory<AccessCodeCastPrefUpdater> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_PREF_UPDATER_H_
