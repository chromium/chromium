// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_pref_updater.h"

#include "base/json/values_util.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_media_sink_util.h"

namespace media_router {

AccessCodeCastPrefUpdater::~AccessCodeCastPrefUpdater() = default;

void AccessCodeCastPrefUpdater::GetMediaSinkInternalValueBySinkId(
    const MediaSink::Id sink_id,
    base::OnceCallback<void(base::Value::Dict)> get_sink_callback) {
  GetDevicesDict(base::BindOnce(
      [](const MediaSink::Id sink_id,
         base::OnceCallback<void(base::Value::Dict)> get_sink_callback,
         base::Value::Dict dict) {
        auto* device_dict = dict.FindDict(sink_id);
        std::move(get_sink_callback)
            .Run(device_dict ? std::move(*device_dict) : base::Value::Dict());
      },
      sink_id, std::move(get_sink_callback)));
}

void AccessCodeCastPrefUpdater::GetDeviceAddedTime(
    const MediaSink::Id sink_id,
    base::OnceCallback<void(std::optional<base::Time>)>
        get_device_added_time_callback) {
  GetDeviceAddedTimeDict(base::BindOnce(
      [](const MediaSink::Id sink_id,
         base::OnceCallback<void(std::optional<base::Time>)>
             get_device_added_time_callback,
         base::Value::Dict device_added_time_dict) {
        auto* device_added_time_value = device_added_time_dict.Find(sink_id);
        std::move(get_device_added_time_callback)
            .Run(device_added_time_value
                     ? base::ValueToTime(device_added_time_value)
                     : std::nullopt);
      },
      sink_id, std::move(get_device_added_time_callback)));
}

// static
std::vector<MediaSink::Id> AccessCodeCastPrefUpdater::GetMatchingIPEndPoints(
    const base::Value::Dict& devices_dict,
    net::IPEndPoint ip_endpoint) {
  std::vector<MediaSink::Id> duplicate_sinks;
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

}  // namespace media_router
