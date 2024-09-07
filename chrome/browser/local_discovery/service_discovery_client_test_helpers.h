// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_TEST_HELPERS_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_TEST_HELPERS_H_

#include "chrome/browser/local_discovery/service_discovery_client.h"
#include "chrome/browser/local_discovery/service_discovery_device_lister.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace local_discovery {

class MockServiceDiscoveryClient : public ServiceDiscoveryClient {
 public:
  MockServiceDiscoveryClient();
  ~MockServiceDiscoveryClient() override;

  MOCK_METHOD(void, Start, ());
  MOCK_METHOD(std::unique_ptr<ServiceWatcher>,
              CreateServiceWatcher,
              (const std::string& service_type,
               ServiceWatcher::UpdatedCallback callback),
              (override));

  MOCK_METHOD(std::unique_ptr<ServiceResolver>,
              CreateServiceResolver,
              (const std::string& service_name,
               ServiceResolver::ResolveCompleteCallback callback),
              (override));

  MOCK_METHOD(std::unique_ptr<LocalDomainResolver>,
              CreateLocalDomainResolver,
              (const std::string& domain,
               net::AddressFamily address_family,
               LocalDomainResolver::IPAddressCallback callback),
              (override));
};

class MockServiceWatcher : public ServiceWatcher {
 public:
  explicit MockServiceWatcher(UpdatedCallback callback);
  ~MockServiceWatcher() override;

  MOCK_METHOD(void, Start, (), (override));
  MOCK_METHOD(void, DiscoverNewServices, (), (override));
  MOCK_METHOD(void,
              SetActivelyRefreshServices,
              (bool actively_refresh_services),
              (override));
  MOCK_METHOD(std::string, GetServiceType, (), (const, override));

  void SimulateServiceUpdated(UpdateType update, std::string_view service_name);

 private:
  UpdatedCallback updated_callback_;
};

class MockServiceResolver : public ServiceResolver {
 public:
  explicit MockServiceResolver(
      ServiceResolver::ResolveCompleteCallback callback);
  ~MockServiceResolver() override;

  MOCK_METHOD(void, StartResolving, ());
  MOCK_METHOD(std::string, GetName, (), (const));

  void SimulateResolveComplete(RequestStatus, const ServiceDescription&);

 private:
  ServiceResolver::ResolveCompleteCallback resolve_complete_callback_;
};

class MockServiceDiscoveryDeviceListerDelegate
    : public ServiceDiscoveryDeviceLister::Delegate {
 public:
  MockServiceDiscoveryDeviceListerDelegate();
  ~MockServiceDiscoveryDeviceListerDelegate();

  MOCK_METHOD(void,
              OnDeviceChanged,
              (const std::string& service_type,
               bool added,
               const ServiceDescription& service_description),
              (override));

  MOCK_METHOD(void,
              OnDeviceRemoved,
              (const std::string& service_type,
               const std::string& service_name),
              (override));

  MOCK_METHOD(void,
              OnDeviceCacheFlushed,
              (const std::string& service_type),
              (override));
  MOCK_METHOD(void, OnPermissionRejected, (), (override));
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_TEST_HELPERS_H_
