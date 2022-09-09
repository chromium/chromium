// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_FETCHER_MOCK_WIN_NETWORK_FETCHER_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_FETCHER_MOCK_WIN_NETWORK_FETCHER_FACTORY_H_

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/fetcher/win_network_fetcher_factory.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors {
namespace test {

// Mocked implementation of the WinNetworkFetcherFactory interface.
class MockWinNetworkFetcherFactory : public WinNetworkFetcherFactory {
 public:
  MockWinNetworkFetcherFactory();
  ~MockWinNetworkFetcherFactory() override;

  MOCK_METHOD(std::unique_ptr<WinNetworkFetcher>,
              CreateNetworkFetcher,
              (const GURL&,
               const std::string&,
               (base::flat_map<std::string, std::string>)),
              (override));
};

}  // namespace test
}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_FETCHER_MOCK_WIN_NETWORK_FETCHER_FACTORY_H_
