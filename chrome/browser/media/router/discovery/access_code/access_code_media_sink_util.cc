// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_media_sink_util.h"

#include <map>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/media_router/common/providers/cast/channel/cast_device_capability.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/port_util.h"
#include "net/base/url_util.h"

namespace media_router {

namespace {

using cast_channel::CastDeviceCapability;
using cast_channel::CastDeviceCapabilitySet;

constexpr char kSinkDictKey[] = "sink";
constexpr char kSinkIdKey[] = "sink_id";
constexpr char kDisplayNameKey[] = "display_name";
constexpr char kExtraDataDictKey[] = "extra_data";
constexpr char kCapabilitiesKey[] = "capabilities";
constexpr char kPortKey[] = "port";
constexpr char kIpAddressKey[] = "ip_address";
constexpr char kModelName[] = "model_name";
constexpr char kDefaultAccessCodeModelName[] = "Chromecast Cast Moderator";

CastDeviceCapabilitySet ConvertDeviceCapabilities(
    chrome_browser_media::proto::DeviceCapabilities proto) {
  CastDeviceCapabilitySet capabilities;
  if (proto.video_out())
    capabilities.Put(CastDeviceCapability::kVideoOut);
  if (proto.video_in())
    capabilities.Put(CastDeviceCapability::kVideoIn);
  if (proto.audio_out())
    capabilities.Put(CastDeviceCapability::kAudioOut);
  if (proto.audio_in())
    capabilities.Put(CastDeviceCapability::kAudioIn);
  if (proto.dev_mode())
    capabilities.Put(CastDeviceCapability::kDevMode);
  return capabilities;
}

std::optional<net::IPAddress> GetIPAddress(NetworkInfo network_info) {
  net::IPAddress ip_address;
  const std::string& ip_v6_address = network_info.ip_v6_address();
  const std::string& ip_v4_address = network_info.ip_v4_address();

  // Prioritize using IP_V6 over IP_V4
  if (!ip_v6_address.empty() && ip_address.AssignFromIPLiteral(ip_v6_address)) {
    return ip_address;
  } else if (!ip_v4_address.empty() &&
             ip_address.AssignFromIPLiteral(ip_v4_address)) {
    return ip_address;
  }
  return std::nullopt;
}
}  // namespace

std::pair<std::optional<MediaSinkInternal>, CreateCastMediaSinkResult>
CreateAccessCodeMediaSink(const DiscoveryDevice& discovery_device) {
  if (!discovery_device.has_network_info()) {
    return std::make_pair(std::nullopt,
                          CreateCastMediaSinkResult::kMissingNetworkInfo);
  }
  std::optional<net::IPAddress> ip_address =
      GetIPAddress(discovery_device.network_info());
  if (!ip_address.has_value()) {
    return std::make_pair(
        std::nullopt, CreateCastMediaSinkResult::kMissingOrInvalidIPAddress);
  }

  const std::string& unique_id = discovery_device.id();
  if (unique_id.empty()) {
    return std::make_pair(std::nullopt, CreateCastMediaSinkResult::kMissingID);
  }

  const std::string& display_name = discovery_device.display_name();
  if (display_name.empty()) {
    return std::make_pair(std::nullopt,
                          CreateCastMediaSinkResult::kMissingFriendlyName);
  }

  CastSinkExtraData extra_data;
  const std::string& port = discovery_device.network_info().port();
  int port_value = kCastControlPort;
  // Convert port from string to int
  if (!port.empty() && !base::StringToInt(port, &port_value)) {
    return std::make_pair(std::nullopt,
                          CreateCastMediaSinkResult::kMissingOrInvalidPort);
  }
  if (!net::IsPortValid(port_value)) {
    return std::make_pair(std::nullopt,
                          CreateCastMediaSinkResult::kMissingOrInvalidPort);
  }
  uint16_t valid_port = static_cast<uint16_t>(port_value);
  extra_data.ip_endpoint = net::IPEndPoint(ip_address.value(), valid_port);
  if (!discovery_device.has_device_capabilities()) {
    return std::make_pair(
        std::nullopt, CreateCastMediaSinkResult::kMissingDeviceCapabilities);
  }
  extra_data.capabilities =
      ConvertDeviceCapabilities(discovery_device.device_capabilities());
  extra_data.discovery_type = CastDiscoveryType::kAccessCodeManualEntry;
  // Various pieces of Chrome make decisions about how to support casting based
  // on the device's model name, generally speaking treating anything that
  // starts with "Chromecast" as a first party casting device.
  extra_data.model_name = kDefaultAccessCodeModelName;

  const std::string& processed_uuid =
      MediaSinkInternal::ProcessDeviceUUID(unique_id);
  const std::string& sink_id =
      base::StringPrintf("cast:<%s>", processed_uuid.c_str());
  MediaSink sink(sink_id, display_name,
                 GetCastSinkIconType(extra_data.capabilities),
                 mojom::MediaRouteProviderId::CAST);

  MediaSinkInternal cast_sink;
  cast_sink.set_sink(sink);
  cast_sink.set_cast_data(extra_data);

  return std::make_pair(cast_sink, CreateCastMediaSinkResult::kOk);
}

base::Value::Dict CreateValueDictFromMediaSinkInternal(
    const MediaSinkInternal& sink) {
  const CastSinkExtraData& extra_data = sink.cast_data();

  base::Value::Dict extra_data_dict;
  extra_data_dict.Set(
      kCapabilitiesKey,
      static_cast<int>(extra_data.capabilities.ToEnumBitmask()));
  extra_data_dict.Set(kPortKey, extra_data.ip_endpoint.port());
  extra_data_dict.Set(kIpAddressKey,
                      extra_data.ip_endpoint.address().ToString());
  extra_data_dict.Set(kModelName, extra_data.model_name);

  base::Value::Dict sink_dict;
  sink_dict.Set(kSinkIdKey, sink.id());
  sink_dict.Set(kDisplayNameKey, sink.sink().name());

  base::Value::Dict value_dict;
  value_dict.Set(kSinkDictKey, std::move(sink_dict));
  value_dict.Set(kExtraDataDictKey, std::move(extra_data_dict));

  return value_dict;
}

// This stored dict looks like:
//   "<cast1>:1234234": {
//     "sink": {
//       "sink_id": "<cast1>:1234234",
//       "display_name": "Karls Cast Device",
//     },
//     "extra_data": {
//       "capabilities": 4,
//       "port": 666,
//       "ip_address": ""192.0.2.146"",
//     },
//   }
std::optional<MediaSinkInternal> ParseValueDictIntoMediaSinkInternal(
    const base::Value::Dict& value_dict) {
  const auto* extra_data_dict = value_dict.FindDict(kExtraDataDictKey);
  if (!extra_data_dict)
    return std::nullopt;

  net::IPAddress ip_address;
  const std::string* ip_address_string =
      extra_data_dict->FindString(kIpAddressKey);
  if (!ip_address_string)
    return std::nullopt;
  if (!ip_address.AssignFromIPLiteral(*ip_address_string))
    return std::nullopt;

  std::optional<int> port = extra_data_dict->FindInt(kPortKey);
  if (!port.has_value())
    return std::nullopt;

  std::optional<int> capabilities = extra_data_dict->FindInt(kCapabilitiesKey);
  if (!capabilities.has_value())
    return std::nullopt;

  CastSinkExtraData extra_data;
  extra_data.ip_endpoint = net::IPEndPoint(ip_address, port.value());
  extra_data.capabilities =
      CastDeviceCapabilitySet::FromEnumBitmask(capabilities.value());
  extra_data.discovery_type = CastDiscoveryType::kAccessCodeRememberedDevice;
  const std::string* model_name = extra_data_dict->FindString(kModelName);
  extra_data.model_name =
      model_name ? *model_name : kDefaultAccessCodeModelName;

  const auto* sink_dict = value_dict.FindDict(kSinkDictKey);
  if (!sink_dict)
    return std::nullopt;
  const std::string* sink_id = sink_dict->FindString(kSinkIdKey);
  if (!sink_id)
    return std::nullopt;
  const std::string* display_name = sink_dict->FindString(kDisplayNameKey);
  if (!display_name)
    return std::nullopt;

  MediaSink sink(*sink_id, *display_name,
                 GetCastSinkIconType(extra_data.capabilities),
                 mojom::MediaRouteProviderId::CAST);

  MediaSinkInternal cast_sink;
  cast_sink.set_sink(sink);
  cast_sink.set_cast_data(extra_data);

  return cast_sink;
}

AccessCodeCastAddSinkResult AddSinkResultMetricsHelper(
    AddSinkResultCode value) {
  switch (value) {
    case AddSinkResultCode::UNKNOWN_ERROR:
      return AccessCodeCastAddSinkResult::kUnknownError;
    case AddSinkResultCode::OK:
      return AccessCodeCastAddSinkResult::kOk;
    case AddSinkResultCode::AUTH_ERROR:
      return AccessCodeCastAddSinkResult::kAuthError;
    case AddSinkResultCode::HTTP_RESPONSE_CODE_ERROR:
      return AccessCodeCastAddSinkResult::kHttpResponseCodeError;
    case AddSinkResultCode::RESPONSE_MALFORMED:
      return AccessCodeCastAddSinkResult::kResponseMalformed;
    case AddSinkResultCode::EMPTY_RESPONSE:
      return AccessCodeCastAddSinkResult::kEmptyResponse;
    case AddSinkResultCode::INVALID_ACCESS_CODE:
      return AccessCodeCastAddSinkResult::kInvalidAccessCode;
    case AddSinkResultCode::ACCESS_CODE_NOT_FOUND:
      return AccessCodeCastAddSinkResult::kAccessCodeNotFound;
    case AddSinkResultCode::TOO_MANY_REQUESTS:
      return AccessCodeCastAddSinkResult::kTooManyRequests;
    case AddSinkResultCode::SERVICE_NOT_PRESENT:
      return AccessCodeCastAddSinkResult::kServiceNotPresent;
    case AddSinkResultCode::SERVER_ERROR:
      return AccessCodeCastAddSinkResult::kServerError;
    case AddSinkResultCode::SINK_CREATION_ERROR:
      return AccessCodeCastAddSinkResult::kSinkCreationError;
    case AddSinkResultCode::CHANNEL_OPEN_ERROR:
      return AccessCodeCastAddSinkResult::kChannelOpenError;
    case AddSinkResultCode::PROFILE_SYNC_ERROR:
      return AccessCodeCastAddSinkResult::kProfileSyncError;
    case AddSinkResultCode::INTERNAL_MEDIA_ROUTER_ERROR:
      return AccessCodeCastAddSinkResult::kInternalMediaRouterError;
  }
  NOTREACHED();
}

std::optional<net::IPEndPoint> GetIPEndPointFromValueDict(
    const base::Value::Dict& value_dict) {
  const auto* extra_data_dict = value_dict.FindDict(kExtraDataDictKey);
  if (!extra_data_dict) {
    return std::nullopt;
  }

  net::IPAddress ip_address;
  const std::string* ip_address_string =
      extra_data_dict->FindString(kIpAddressKey);
  if (!ip_address_string) {
    return std::nullopt;
  }
  if (!ip_address.AssignFromIPLiteral(*ip_address_string)) {
    return std::nullopt;
  }

  std::optional<int> port = extra_data_dict->FindInt(kPortKey);
  if (!port.has_value()) {
    return std::nullopt;
  }

  return net::IPEndPoint(ip_address, port.value());
}

}  // namespace media_router
