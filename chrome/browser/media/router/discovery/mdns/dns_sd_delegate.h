// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_DNS_SD_DELEGATE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_DNS_SD_DELEGATE_H_

#include <string>
#include <vector>

#include "net/base/host_port_pair.h"

namespace media_router {

struct DnsSdService {
  std::string service_name;
  net::HostPortPair service_host_port;
  std::string ip_address;
  std::vector<std::string> service_data;

  DnsSdService();
  DnsSdService(const DnsSdService& other);
  virtual ~DnsSdService();

  bool operator==(const DnsSdService& other) const {
    return service_name == other.service_name &&
           service_host_port.Equals(other.service_host_port) &&
           ip_address == other.ip_address && service_data == other.service_data;
  }

  bool operator!=(const DnsSdService& other) const { return !(*this == other); }
};

// Delegate that is notified when a watched service is added, updated or
// removed.
class DnsSdDelegate {
 public:
  virtual void ServiceChanged(const std::string& service_type,
                              bool added,
                              const DnsSdService& service) = 0;
  virtual void ServiceRemoved(const std::string& service_type,
                              const std::string& service_name) = 0;
  virtual void ServicesFlushed(const std::string& service_type) = 0;
  virtual void ServicesPermissionRejected() = 0;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_DNS_SD_DELEGATE_H_
