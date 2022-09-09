// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/fetcher/win_network_fetcher_factory.h"

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/fetcher/win_network_fetcher_impl.h"

namespace enterprise_connectors {

namespace {

// Implementation of the WinNetworkFetcherFactory interface.
class WinNetworkFetcherFactoryImpl : public WinNetworkFetcherFactory {
 public:
  WinNetworkFetcherFactoryImpl();
  ~WinNetworkFetcherFactoryImpl() override;

  std::unique_ptr<WinNetworkFetcher> CreateNetworkFetcher(
      const GURL& url,
      const std::string& body,
      base::flat_map<std::string, std::string> headers) override;
};

WinNetworkFetcherFactoryImpl::WinNetworkFetcherFactoryImpl() {}
WinNetworkFetcherFactoryImpl::~WinNetworkFetcherFactoryImpl() = default;

std::unique_ptr<WinNetworkFetcher>
WinNetworkFetcherFactoryImpl::CreateNetworkFetcher(
    const GURL& url,
    const std::string& body,
    base::flat_map<std::string, std::string> headers) {
  return std::make_unique<WinNetworkFetcherImpl>(url, body, headers);
}

}  // namespace

// static
std::unique_ptr<WinNetworkFetcherFactory> WinNetworkFetcherFactory::Create() {
  return std::make_unique<WinNetworkFetcherFactoryImpl>();
}

}  // namespace enterprise_connectors
