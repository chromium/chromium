// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MAC_UTIL_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MAC_UTIL_H_

#import <Foundation/Foundation.h>

#include <optional>
#include <string>
#include <vector>

namespace local_discovery {
struct ServiceDescription;

struct ServiceInfo {
  std::optional<std::string> instance;
  std::optional<std::string> sub_type;
  std::string service_type;
  std::string domain;

  ServiceInfo();
  ServiceInfo(const ServiceInfo&);
  ServiceInfo(ServiceInfo&& other);
  ServiceInfo& operator=(const ServiceInfo& other);
  ServiceInfo& operator=(ServiceInfo&& other);
  ~ServiceInfo();
};

std::ostream& operator<<(std::ostream& stream, const ServiceInfo& service);

std::optional<ServiceInfo> ExtractServiceInfo(const std::string& service,
                                              bool is_service_name);

void ParseTxtRecord(NSData* record, std::vector<std::string>* output);

void ParseNetService(NSNetService* service, ServiceDescription& description);
}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MAC_UTIL_H_
