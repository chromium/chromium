// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_TEST_SERVICE_DISCOVERY_CLIENT_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_TEST_SERVICE_DISCOVERY_CLIENT_H_

#include <stdint.h>

#include "chrome/browser/local_discovery/service_discovery_client.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "net/dns/mdns_client.h"
#include "net/dns/mock_mdns_socket_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace local_discovery {

class TestServiceDiscoveryClient : public ServiceDiscoverySharedClient {
 public:
  TestServiceDiscoveryClient();

  void Start();

  std::unique_ptr<ServiceWatcher> CreateServiceWatcher(
      const std::string& service_type,
      ServiceWatcher::UpdatedCallback callback) override;
  std::unique_ptr<ServiceResolver> CreateServiceResolver(
      const std::string& service_name,
      ServiceResolver::ResolveCompleteCallback callback) override;
  std::unique_ptr<LocalDomainResolver> CreateLocalDomainResolver(
      const std::string& domain,
      net::AddressFamily address_family,
      LocalDomainResolver::IPAddressCallback callback) override;

  MOCK_METHOD1(OnSendTo, void(const std::string& data));

  void SimulateReceive(const uint8_t* packet, int size);

 private:
  ~TestServiceDiscoveryClient() override;

  // Owned by mdns_client_impl_.
  net::MockMDnsSocketFactory mock_socket_factory_;
  std::unique_ptr<net::MDnsClient> mdns_client_;
  std::unique_ptr<ServiceDiscoveryClient> service_discovery_client_impl_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_TEST_SERVICE_DISCOVERY_CLIENT_H_
