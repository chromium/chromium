// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/test_service_discovery_client.h"

#include "base/logging.h"
#include "chrome/browser/local_discovery/service_discovery_client_impl.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/dns/mdns_client_impl.h"

namespace local_discovery {

TestServiceDiscoveryClient::TestServiceDiscoveryClient() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

TestServiceDiscoveryClient::~TestServiceDiscoveryClient() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void TestServiceDiscoveryClient::Start() {
  mdns_client_.reset(new net::MDnsClientImpl());
  service_discovery_client_impl_.reset(new ServiceDiscoveryClientImpl(
      mdns_client_.get()));
  int result = mdns_client_->StartListening(&mock_socket_factory_);
  DCHECK_EQ(net::OK, result);

  EXPECT_CALL(mock_socket_factory_, OnSendTo(testing::_))
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Invoke(this,
                                      &TestServiceDiscoveryClient::OnSendTo));
}

std::unique_ptr<ServiceWatcher>
TestServiceDiscoveryClient::CreateServiceWatcher(
    const std::string& service_type,
    ServiceWatcher::UpdatedCallback callback) {
  return service_discovery_client_impl_->CreateServiceWatcher(
      service_type, std::move(callback));
}

std::unique_ptr<ServiceResolver>
TestServiceDiscoveryClient::CreateServiceResolver(
    const std::string& service_name,
    ServiceResolver::ResolveCompleteCallback callback) {
  return service_discovery_client_impl_->CreateServiceResolver(
      service_name, std::move(callback));
}

std::unique_ptr<LocalDomainResolver>
TestServiceDiscoveryClient::CreateLocalDomainResolver(
    const std::string& domain,
    net::AddressFamily address_family,
    LocalDomainResolver::IPAddressCallback callback) {
  return service_discovery_client_impl_->CreateLocalDomainResolver(
      domain, address_family, std::move(callback));
}

void TestServiceDiscoveryClient::SimulateReceive(const uint8_t* packet,
                                                 int size) {
  mock_socket_factory_.SimulateReceive(packet, size);
}

}  // namespace local_discovery
