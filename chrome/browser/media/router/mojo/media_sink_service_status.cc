// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_sink_service_status.h"

#include "base/json/json_string_value_serializer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"

namespace media_router {

namespace {

constexpr size_t kMaxAvailableSinksSize = 100;
constexpr char kCastPrefix[] = "cast:<";
constexpr char kDialPrefix[] = "dial:<";

// Helper function to convert |value| to JSON string.
std::string ToJSONString(const base::Value::Dict& value) {
  std::string json;
  JSONStringValueSerializer serializer(&json);
  serializer.set_pretty_print(true);
  return serializer.Serialize(value) ? json : "";
}

// Returns UUID if |sink_id| is in the format of "cast:<UUID>" or "dial:<UUID>";
// otherwise returns |sink_id| as UUID.
base::StringPiece ExtractUUID(const base::StringPiece& sink_id) {
  if (!base::EndsWith(sink_id, ">"))
    return sink_id;

  size_t prefix_length = 0;
  if (base::StartsWith(sink_id, kCastPrefix))
    prefix_length = sizeof(kCastPrefix) - 1;
  if (base::StartsWith(sink_id, kDialPrefix))
    prefix_length = sizeof(kDialPrefix) - 1;

  if (prefix_length == 0)
    return sink_id;

  base::StringPiece uuid = sink_id;
  uuid.remove_prefix(prefix_length);
  uuid.remove_suffix(1);
  return uuid;
}

// Returns the last four characters of UUID. UUID is extracted from |sink_id|.
std::string TruncateSinkId(const std::string& sink_id) {
  std::string uuid(ExtractUUID(sink_id));
  return uuid.length() <= 4 ? uuid : uuid.substr(uuid.length() - 4);
}

// Helper function to convert |sink_internal| to JSON format represented by
// base::Value::Dict.
base::Value::Dict ToValue(const MediaSinkInternal& sink_internal) {
  base::Value::Dict dict;
  const MediaSink& sink = sink_internal.sink();
  dict.Set("id", base::Value(TruncateSinkId(sink.id())));
  dict.Set("name", base::Value(sink.name()));
  if (sink.description())
    dict.Set("description", base::Value(*sink.description()));
  if (sink.domain())
    dict.Set("domain", base::Value(*sink.domain()));
  dict.Set("icon_type", base::Value(static_cast<int>(sink.icon_type())));

  if (sink_internal.is_dial_sink()) {
    DialSinkExtraData extra_data = sink_internal.dial_data();
    dict.Set("ip_address", base::Value(extra_data.ip_address.ToString()));
    dict.Set("model_name", base::Value(extra_data.model_name));
    dict.Set("app_url", base::Value(extra_data.app_url.spec()));
  }

  if (sink_internal.is_cast_sink()) {
    CastSinkExtraData extra_data = sink_internal.cast_data();
    dict.Set("ip_endpoint", base::Value(extra_data.ip_endpoint.ToString()));
    dict.Set("model_name", base::Value(extra_data.model_name));
    dict.Set("capabilities", base::Value(extra_data.capabilities));
    dict.Set("channel_id", base::Value(extra_data.cast_channel_id));
    dict.Set("discovered_by_dial", base::Value(extra_data.discovery_type ==
                                               CastDiscoveryType::kDial));
  }
  return dict;
}

// Helper function to convert |sinks| to JSON format represented by
// base::Value::Dict.
base::Value::Dict ConvertDiscoveredSinksToValues(
    const base::flat_map<std::string, std::vector<MediaSinkInternal>>& sinks) {
  base::Value::Dict dict;
  for (const auto& sinks_it : sinks) {
    base::Value::List list;
    for (const auto& inner_sink : sinks_it.second)
      list.Append(ToValue(inner_sink));
    dict.Set(sinks_it.first, std::move(list));
  }
  return dict;
}

// Helper function to convert |available_sinks| to a dictionary of availability
// strings in JSON format represented by base::Value::Dict.
base::Value::Dict ConvertAvailableSinksToValues(
    const base::LRUCache<std::string, std::vector<MediaSinkInternal>>&
        available_sinks) {
  base::Value::Dict dict;
  for (const auto& sinks_it : available_sinks) {
    base::Value::List list;
    for (const auto& inner_sink : sinks_it.second) {
      std::string sink_id = inner_sink.sink().id();
      list.Append(base::Value(TruncateSinkId(sink_id)));
    }
    dict.Set(sinks_it.first, std::move(list));
  }
  return dict;
}

}  // namespace

MediaSinkServiceStatus::MediaSinkServiceStatus()
    : available_sinks_(kMaxAvailableSinksSize) {}
MediaSinkServiceStatus::~MediaSinkServiceStatus() = default;

void MediaSinkServiceStatus::UpdateDiscoveredSinks(
    const std::string& provider_name,
    const std::vector<MediaSinkInternal>& discovered_sinks) {
  discovered_sinks_[provider_name] = discovered_sinks;
}

void MediaSinkServiceStatus::UpdateAvailableSinks(
    mojom::MediaRouteProviderId provider_id,
    const std::string& media_source,
    const std::vector<MediaSinkInternal>& available_sinks) {
  // TODO(takumif): It'd be safer and more efficient to make use
  // pair<MediaRouteProviderId, string> than a serialized "id:name" string.
  std::string key =
      base::StrCat({ProviderIdToString(provider_id), ":", media_source});
  available_sinks_.Put(key, available_sinks);
}

base::Value::Dict MediaSinkServiceStatus::GetStatusAsValue() const {
  base::Value::Dict status_dict;
  status_dict.Set("discovered_sinks",
                  ConvertDiscoveredSinksToValues(discovered_sinks_));
  status_dict.Set("available_sinks",
                  ConvertAvailableSinksToValues(available_sinks_));

  return status_dict;
}

std::string MediaSinkServiceStatus::GetStatusAsJSONString() const {
  return ToJSONString(GetStatusAsValue());
}

}  // namespace media_router
