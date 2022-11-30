// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_FETCHER_WIN_NETWORK_FETCHER_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_FETCHER_WIN_NETWORK_FETCHER_FACTORY_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/fetcher/win_network_fetcher.h"

class GURL;

namespace enterprise_connectors {

// Factory responsible for the creation of the WinNetworkFetcher object.
class WinNetworkFetcherFactory {
 public:
  virtual ~WinNetworkFetcherFactory() = default;

  // Creates a WinNetworkFetcherFactory instance.
  static std::unique_ptr<WinNetworkFetcherFactory> Create();

  // Creates the WinNetworkFetcher that is used to issue a
  // DeviceManagementRequest to the DM server using the DM server `url`, HTTP
  // `headers`, and `body`.
  virtual std::unique_ptr<WinNetworkFetcher> CreateNetworkFetcher(
      const GURL& url,
      const std::string& body,
      base::flat_map<std::string, std::string> headers) = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_FETCHER_WIN_NETWORK_FETCHER_FACTORY_H_
