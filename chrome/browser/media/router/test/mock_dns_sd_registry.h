// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_TEST_MOCK_DNS_SD_REGISTRY_H_
#define CHROME_BROWSER_MEDIA_ROUTER_TEST_MOCK_DNS_SD_REGISTRY_H_

#include "chrome/browser/media/router/discovery/mdns/dns_sd_registry.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {

class MockDnsSdRegistry : public DnsSdRegistry {
 public:
  explicit MockDnsSdRegistry(DnsSdObserver* observer);
  ~MockDnsSdRegistry() override;

  MOCK_METHOD1(AddObserver, void(DnsSdObserver* observer));
  MOCK_METHOD1(RemoveObserver, void(DnsSdObserver* observer));
  MOCK_METHOD1(RegisterDnsSdListener, void(const std::string& service_type));
  MOCK_METHOD1(UnregisterDnsSdListener, void(const std::string& service_type));
  MOCK_METHOD1(Publish, void(const std::string&));
  MOCK_METHOD0(ResetAndDiscover, void(void));

  void DispatchMDnsEvent(const std::string& service_type,
                         const DnsSdServiceList& services);

 private:
  DnsSdObserver* observer_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_TEST_MOCK_DNS_SD_REGISTRY_H_
