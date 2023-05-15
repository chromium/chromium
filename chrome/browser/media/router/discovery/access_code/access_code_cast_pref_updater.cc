// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_pref_updater.h"

#include "base/json/values_util.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_media_sink_util.h"

namespace media_router {

AccessCodeCastPrefUpdater::~AccessCodeCastPrefUpdater() = default;

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

}  // namespace media_router
