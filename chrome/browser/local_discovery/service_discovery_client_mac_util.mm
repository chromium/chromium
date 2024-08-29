// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/service_discovery_client_mac_util.h"

#include "base/apple/foundation_util.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/local_discovery/service_discovery_client.h"
#include "net/base/ip_endpoint.h"

namespace local_discovery {

ServiceInfo::ServiceInfo() = default;
ServiceInfo::ServiceInfo(const ServiceInfo&) = default;
ServiceInfo::ServiceInfo(ServiceInfo&& other) = default;
ServiceInfo& ServiceInfo::operator=(const ServiceInfo& other) = default;
ServiceInfo& ServiceInfo::operator=(ServiceInfo&& other) = default;
ServiceInfo::~ServiceInfo() = default;

std::ostream& operator<<(std::ostream& stream, const ServiceInfo& service) {
  if (service.instance) {
    stream << "instance: '" << service.instance.value() << "', ";
  }
  return stream << "'" << service.service_type << "', domain: '"
                << service.domain << "'";
}

// Extracts the instance name, service type and domain from a full service name
// or the service type and domain from a service type. Returns std::nullopt if
// `service` is not valid.
// Examples: '<instance_name>._<service_type2>._<service_type1>.<domain>',
// '<instance_name>._<sub_type>._sub._<service_type2>._<service_type1>.<domain>'
// Reference: https://datatracker.ietf.org/doc/html/rfc6763#section-4.1
std::optional<ServiceInfo> ExtractServiceInfo(const std::string& service,
                                              bool is_service_name) {
  if (service.empty() || !base::IsStringUTF8(service)) {
    return std::nullopt;
  }
  ServiceInfo info;

  std::vector<std::string_view> tokens = base::SplitStringPiece(
      service, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  size_t num_tokens = tokens.size();
  bool has_subtype = num_tokens >= 4 && tokens[num_tokens - 4] == "_sub";
  size_t minimum_num_tokens =
      (is_service_name ? 1 : 0) + (has_subtype ? 2 : 0) + 3;
  if (num_tokens < minimum_num_tokens) {
    return std::nullopt;
  }

  if (is_service_name) {
    std::vector<std::string_view>::iterator instance_name_end_it =
        has_subtype ? tokens.end() - 5 : tokens.end() - 3;
    info.instance = base::JoinString(
        std::vector(tokens.begin(), instance_name_end_it), ".");
  }
  if (has_subtype) {
    info.sub_type = base::StrCat({tokens[num_tokens - 5], "."});
  }

  info.service_type =
      base::StrCat({*(tokens.end() - 3), ".", *(tokens.end() - 2), "."});
  info.domain = base::StrCat({tokens.back(), "."});
  if (info.domain.size() < 1 || info.service_type.size() < 1 ||
      (is_service_name && !info.instance)) {
    return std::nullopt;
  } else {
    return info;
  }
}

void ParseTxtRecord(NSData* record, std::vector<std::string>& output) {
  size_t size = base::strict_cast<size_t>(record.length);
  if (size <= 1) {
    return;
  }

  VLOG(1) << "ParseTxtRecord: " << size;

  base::span<const uint8_t> bytes_span = base::apple::NSDataToSpan(record);
  size_t offset = 0;
  while (offset < size) {
    size_t record_size = static_cast<size_t>(bytes_span[offset++]);
    if (offset > size - record_size) {
      break;
    }

    std::string_view txt_record_string =
        as_string_view(bytes_span.subspan(offset, record_size));
    if (base::IsStringUTF8(txt_record_string)) {
      VLOG(1) << "TxtRecord: " << txt_record_string;
      output.emplace_back(txt_record_string);
    } else {
      VLOG(1) << "TxtRecord corrupted at offset " << offset;
    }

    offset += record_size;
  }
}

void ParseNetService(NSNetService* service, ServiceDescription& description) {
  for (NSData* address in [service addresses]) {
    const void* bytes = [address bytes];
    int length = [address length];
    const sockaddr* socket = static_cast<const sockaddr*>(bytes);
    net::IPEndPoint end_point;
    if (end_point.FromSockAddr(socket, length)) {
      description.address = net::HostPortPair::FromIPEndPoint(end_point);
      description.ip_address = end_point.address();
      break;
    }
  }

  ParseTxtRecord([service TXTRecordData], description.metadata);
}
}  // namespace local_discovery
