// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_FETCHER_MOCK_WIN_NETWORK_FETCHER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_FETCHER_MOCK_WIN_NETWORK_FETCHER_H_

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/fetcher/win_network_fetcher.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors {
namespace test {

// Mocked implementation of the WinNetworkFetcher interface.
class MockWinNetworkFetcher : public WinNetworkFetcher {
 public:
  MockWinNetworkFetcher();
  ~MockWinNetworkFetcher() override;

  MOCK_METHOD(void, Fetch, (FetchCompletedCallback), (override));
};

}  // namespace test
}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_FETCHER_MOCK_WIN_NETWORK_FETCHER_H_
