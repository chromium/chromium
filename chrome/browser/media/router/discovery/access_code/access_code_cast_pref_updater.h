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

// An interface used for pref updating in AccessCodeCasting.
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
  virtual void UpdateDevicesDict(const MediaSinkInternal& sink,
                                 base::OnceClosure on_updated_callback) = 0;

  // Sets the key for the |sink_id| with the time it is added. This is
  // calculated at the time of the functions calling. If the |sink_id| already
  // exist, then update the value of that |sink_id| with a new time.
  virtual void UpdateDeviceAddedTimeDict(
      const MediaSink::Id sink_id,
      base::OnceClosure on_updated_callback) = 0;

  // Runs the |get_devices_callback| with the devices dictionary fetched from
  // the pref service.
  virtual void GetDevicesDict(
      base::OnceCallback<void(base::Value::Dict)> get_devices_callback) = 0;

  // Runs the |get_device_added_time_callback| with the device added time
  // dictionary fetched from the pref service.
  virtual void GetDeviceAddedTimeDict(
      base::OnceCallback<void(base::Value::Dict)>
          get_device_added_time_callback) = 0;

  // Removes the given |sink_id| from all instances in the devices dictionary
  // stored in the pref service. Nothing occurs if the |sink_id| was not there
  // in the first place.
  virtual void RemoveSinkIdFromDevicesDict(
      const MediaSink::Id sink_id,
      base::OnceClosure on_sink_removed_callback) = 0;

  // Removes the given |sink_id| from all instances in the device Added
  // dictionary stored in the pref service. Nothing occurs if the |sink_id| was
  // not there in the first place.
  virtual void RemoveSinkIdFromDeviceAddedTimeDict(
      const MediaSink::Id sink_id,
      base::OnceClosure on_sink_removed_callback) = 0;

  virtual void ClearDevicesDict(base::OnceClosure on_cleared_callback) = 0;
  virtual void ClearDeviceAddedTimeDict(
      base::OnceClosure on_cleared_callback) = 0;

  // Fetches the devices dictionary from the pref service and finds the entry
  // with |sink_id|. If found a value, runs the |get_sink_callback| with this
  // value, otherwise runs the callback with an empty dictionary.
  void GetMediaSinkInternalValueBySinkId(
      const MediaSink::Id sink_id,
      base::OnceCallback<void(base::Value::Dict)> get_sink_callback);

  // Fetches the devices added time dictionary from the pref service and finds
  // the entry with |sink_id|. If found a valid time, runs the
  // |get_sink_callback| with this value, otherwise runs the callback with
  // absl::nullop.
  void GetDeviceAddedTime(const MediaSink::Id sink_id,
                          base::OnceCallback<void(std::optional<base::Time>)>
                              get_device_added_time_callback);

  // Returns a list of media sink id's of devices in |devices_dict| whose ip
  // endpoints are identical to the given |ip_endpoint|. If no existing ip
  // endpoints are found, the list will be empty.
  static std::vector<MediaSink::Id> GetMatchingIPEndPoints(
      const base::Value::Dict& devices_dict,
      net::IPEndPoint ip_endpoint);

  // Sets the key for the given |sink| id with the actual |sink| itself. This
  // function will overwrite a sink id if it already exists.
  virtual void UpdateDevicesDictForTesting(const MediaSinkInternal& sink) = 0;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_PREF_UPDATER_H_
