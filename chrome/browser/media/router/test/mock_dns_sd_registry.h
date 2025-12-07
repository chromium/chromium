// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_TEST_MOCK_DNS_SD_REGISTRY_H_
#define CHROME_BROWSER_MEDIA_ROUTER_TEST_MOCK_DNS_SD_REGISTRY_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/media/router/discovery/mdns/dns_sd_registry.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {

class MockDnsSdRegistry : public DnsSdRegistry {
 public:
  explicit MockDnsSdRegistry(DnsSdObserver* observer);
  ~MockDnsSdRegistry() override;

  MOCK_METHOD(void, AddObserver, (DnsSdObserver * observer));
  MOCK_METHOD(void, RemoveObserver, (DnsSdObserver * observer));
  MOCK_METHOD(void, RegisterDnsSdListener, (const std::string& service_type));
  MOCK_METHOD(void, UnregisterDnsSdListener, (const std::string& service_type));
  MOCK_METHOD(void, Publish, (const std::string&));
  MOCK_METHOD(void, ResetAndDiscover, ());

  void DispatchMDnsEvent(const std::string& service_type,
                         const DnsSdServiceList& services);
  void SimulatePermissionRejected();

 private:
  raw_ptr<DnsSdObserver, DanglingUntriaged> observer_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_TEST_MOCK_DNS_SD_REGISTRY_H_
