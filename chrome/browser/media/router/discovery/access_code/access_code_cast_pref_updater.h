// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_PREF_UPDATER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_PREF_UPDATER_H_

#include "base/time/time.h"
#include "base/values.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/media_sink.h"

namespace media_router {

// An interface used by both LaCros and other desktop platforms for pref
// updating in AccessCodeCasting.
class AccessCodeCastPrefUpdater {
 public:
  AccessCodeCastPrefUpdater() = default;
  AccessCodeCastPrefUpdater(const AccessCodeCastPrefUpdater&) = delete;
  AccessCodeCastPrefUpdater& operator=(const AccessCodeCastPrefUpdater&) =
      delete;

  virtual ~AccessCodeCastPrefUpdater();

  // Sets the key for the given |sink| id with the actual |sink| itself. This
  // function will overwrite a sink id if it already exists. If ip_endpoints
  // already exist with the given |sink| id, those entries will be removed from
  // the pref service.
  virtual void UpdateDevicesDict(const MediaSinkInternal& sink) = 0;

  // Sets the key for the |sink_id| with the time it is added. This is
  // calculated at the time of the functions calling. If the |sink_id| already
  // exist, then update the value of that |sink_id| with a new time.
  virtual void UpdateDeviceAddedTimeDict(const MediaSink::Id sink_id) = 0;

  // Returns a the device dictionary from the pref service.
  virtual const base::Value::Dict& GetDevicesDict() = 0;

  // Returns a nullptr if the device Added dictionary does not exist in the
  // pref service for some reason.
  virtual const base::Value::Dict& GetDeviceAddedTimeDict() = 0;

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
  virtual void RemoveSinkIdFromDevicesDict(const MediaSink::Id sink_id) = 0;

  // Removes the given |sink_id| from all instances in the device Added
  // dictionary stored in the pref service. Nothing occurs if the |sink_id| was
  // not there in the first place.
  virtual void RemoveSinkIdFromDeviceAddedTimeDict(
      const MediaSink::Id sink_id) = 0;

  // Returns a list of media sink id's of stored media sinks whose ip endpoints
  // are identical to the given ip_endpoint. If no existing ip endpoints are
  // found, the list will be empty.
  std::vector<MediaSink::Id> GetMatchingIPEndPoints(
      net::IPEndPoint ip_endpoint);

  virtual void ClearDevicesDict() = 0;
  virtual void ClearDeviceAddedTimeDict() = 0;

  // Sets the key for the given |sink| id with the actual |sink| itself. This
  // function will overwrite a sink id if it already exists.
  virtual void UpdateDevicesDictForTest(const MediaSinkInternal& sink) = 0;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_PREF_UPDATER_H_
