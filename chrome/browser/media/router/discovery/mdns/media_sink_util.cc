// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/mdns/media_sink_util.h"

#include <map>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/discovery/mdns/dns_sd_delegate.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/url_util.h"

namespace media_router {

SinkIconType GetCastSinkIconType(
    cast_channel::CastDeviceCapabilitySet capabilities) {
  if (capabilities.Has(cast_channel::CastDeviceCapability::kVideoOut)) {
    return SinkIconType::CAST;
  }

  return capabilities.Has(cast_channel::CastDeviceCapability::kMultizoneGroup)
             ? SinkIconType::CAST_AUDIO_GROUP
             : SinkIconType::CAST_AUDIO;
}

CreateCastMediaSinkResult CreateCastMediaSink(const DnsSdService& service,
                                              MediaSinkInternal* cast_sink) {
  DCHECK(cast_sink);
  if (service.service_name.find(kCastServiceType) == std::string::npos)
    return CreateCastMediaSinkResult::kNotCastDevice;

  net::IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(service.ip_address))
    return CreateCastMediaSinkResult::kMissingOrInvalidIPAddress;

  std::map<std::string, std::string> service_data;
  for (const auto& item : service.service_data) {
    // |item| format should be "id=xxxxxx", etc.
    size_t split_idx = item.find('=');
    if (split_idx == std::string::npos)
      continue;

    std::string key = item.substr(0, split_idx);
    std::string val =
        split_idx < item.length() ? item.substr(split_idx + 1) : "";
    service_data[key] = val;
  }

  std::string unique_id = service_data["id"];
  if (unique_id.empty())
    return CreateCastMediaSinkResult::kMissingID;
  std::string friendly_name = service_data["fn"];
  if (friendly_name.empty())
    return CreateCastMediaSinkResult::kMissingFriendlyName;

  CastSinkExtraData extra_data;
  extra_data.ip_endpoint =
      net::IPEndPoint(ip_address, service.service_host_port.port());
  extra_data.model_name = service_data["md"];

  {
    uint64_t capabilities = 0;
    if (base::StringToUint64(service_data["ca"], &capabilities)) {
      extra_data.capabilities =
          cast_channel::CastDeviceCapabilitySet::FromEnumBitmask(capabilities);
    }
  }

  std::string processed_uuid = MediaSinkInternal::ProcessDeviceUUID(unique_id);
  std::string sink_id = base::StringPrintf("cast:%s", processed_uuid.c_str());
  MediaSink sink(sink_id, friendly_name,
                 GetCastSinkIconType(extra_data.capabilities),
                 mojom::MediaRouteProviderId::CAST);

  cast_sink->set_sink(sink);
  cast_sink->set_cast_data(extra_data);

  return CreateCastMediaSinkResult::kOk;
}

std::vector<MediaSinkInternal> GetFixedIPSinksFromCommandLine() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string ips_string =
      command_line->GetSwitchValueASCII(kFixedCastDeviceIps);
  if (ips_string.empty())
    return std::vector<MediaSinkInternal>();

  std::vector<MediaSinkInternal> sinks;
  std::vector<std::string> ips = base::SplitString(
      ips_string, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& ip : ips) {
    std::string host;
    int port = -1;
    if (!net::ParseHostAndPort(ip, &host, &port))
      continue;

    net::IPAddress ip_address;
    if (!ip_address.AssignFromIPLiteral(host))
      continue;

    if (port == -1)
      port = kCastControlPort;

    std::string instance_name;
    base::ReplaceChars(host, ".", "_", &instance_name);
    instance_name = "Receiver-" + instance_name;

    DnsSdService service;
    service.service_name = instance_name + "._googlecast._tcp.local";
    service.service_host_port = net::HostPortPair(host, port);
    service.ip_address = host;
    service.service_data = {
        "id=mdns:" + instance_name, "ve=02",        "ca=5", "st=1",
        "fn=" + instance_name,      "md=Chromecast"};

    MediaSinkInternal sink;
    CreateCastMediaSinkResult result = CreateCastMediaSink(service, &sink);
    if (result == CreateCastMediaSinkResult::kOk) {
      sinks.push_back(sink);
    }
  }
  return sinks;
}

}  // namespace media_router
