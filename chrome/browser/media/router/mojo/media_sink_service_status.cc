// Copyright 2018 The Chromium Authors. All rights reserved.
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
std::string ToJSONString(const base::Value& value) {
  std::string json;
  JSONStringValueSerializer serializer(&json);
  serializer.set_pretty_print(true);
  if (!serializer.Serialize(value)) {
    DVLOG(1) << "Failed to serialize log to JSON.";
    return "";
  }
  return json;
}

// Returns UUID if |sink_id| is in the format of "cast:<UUID>" or "dial:<UUID>";
// otherwise returns |sink_id| as UUID.
base::StringPiece ExtractUUID(const base::StringPiece& sink_id) {
  if (!sink_id.ends_with(">"))
    return sink_id;

  size_t prefix_length = 0;
  if (sink_id.starts_with(kCastPrefix))
    prefix_length = sizeof(kCastPrefix) - 1;
  if (sink_id.starts_with(kDialPrefix))
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
  std::string uuid = ExtractUUID(sink_id).as_string();
  return uuid.length() <= 4 ? uuid : uuid.substr(uuid.length() - 4);
}

// Helper function to convert |sink_internal| to JSON format represented by
// base::Value.
base::Value ToValue(const MediaSinkInternal& sink_internal) {
  base::Value dict(base::Value::Type::DICTIONARY);
  const MediaSink& sink = sink_internal.sink();
  dict.SetKey("id", base::Value(TruncateSinkId(sink.id())));
  dict.SetKey("name", base::Value(sink.name()));
  if (sink.description())
    dict.SetKey("description", base::Value(*sink.description()));
  if (sink.domain())
    dict.SetKey("domain", base::Value(*sink.domain()));
  dict.SetKey("icon_type", base::Value(static_cast<int>(sink.icon_type())));

  if (sink_internal.is_dial_sink()) {
    DialSinkExtraData extra_data = sink_internal.dial_data();
    dict.SetKey("ip_address", base::Value(extra_data.ip_address.ToString()));
    dict.SetKey("model_name", base::Value(extra_data.model_name));
    dict.SetKey("app_url", base::Value(extra_data.app_url.spec()));
  }

  if (sink_internal.is_cast_sink()) {
    CastSinkExtraData extra_data = sink_internal.cast_data();
    dict.SetKey("ip_endpoint", base::Value(extra_data.ip_endpoint.ToString()));
    dict.SetKey("model_name", base::Value(extra_data.model_name));
    dict.SetKey("capabilities", base::Value(extra_data.capabilities));
    dict.SetKey("channel_id", base::Value(extra_data.cast_channel_id));
    dict.SetKey("discovered_by_dial",
                base::Value(extra_data.discovered_by_dial));
  }
  return dict;
}

// Helper function to convert |sinks| to JSON format represented by
// base::Value.
base::Value ConvertDiscoveredSinksToValues(
    const base::flat_map<std::string, std::vector<MediaSinkInternal>>& sinks) {
  base::Value dict(base::Value::Type::DICTIONARY);
  for (const auto& sinks_it : sinks) {
    base::ListValue list;
    for (const auto& inner_sink : sinks_it.second)
      list.Append(ToValue(inner_sink));
    dict.SetKey(sinks_it.first, std::move(list));
  }
  return dict;
}

// Helper function to convert |available_sinks| to a dictionary of availability
// strings in JSON format represented by base::Value.
base::Value ConvertAvailableSinksToValues(
    const base::MRUCache<std::string, std::vector<MediaSinkInternal>>&
        available_sinks) {
  base::Value dict(base::Value::Type::DICTIONARY);
  for (const auto& sinks_it : available_sinks) {
    base::Value list(base::Value::Type::LIST);
    for (const auto& inner_sink : sinks_it.second) {
      std::string sink_id = inner_sink.sink().id();
      list.Append(base::Value(TruncateSinkId(sink_id)));
    }
    dict.SetKey(sinks_it.first, std::move(list));
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
    MediaRouteProviderId provider_id,
    const std::string& media_source,
    const std::vector<MediaSinkInternal>& available_sinks) {
  std::string key =
      base::StrCat({ProviderIdToString(provider_id), ":", media_source});
  available_sinks_.Put(key, available_sinks);
}

base::Value MediaSinkServiceStatus::GetStatusAsValue() const {
  base::Value status_dict(base::Value::Type::DICTIONARY);
  status_dict.SetKey("discovered_sinks",
                     ConvertDiscoveredSinksToValues(discovered_sinks_));
  status_dict.SetKey("available_sinks",
                     ConvertAvailableSinksToValues(available_sinks_));

  return status_dict;
}

std::string MediaSinkServiceStatus::GetStatusAsJSONString() const {
  return ToJSONString(GetStatusAsValue());
}

}  // namespace media_router
