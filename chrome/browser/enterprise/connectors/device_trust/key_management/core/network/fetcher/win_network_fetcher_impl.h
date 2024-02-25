// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_FETCHER_WIN_NETWORK_FETCHER_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_FETCHER_WIN_NETWORK_FETCHER_IMPL_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/fetcher/win_network_fetcher.h"
#include "components/winhttp/network_fetcher.h"
#include "components/winhttp/scoped_hinternet.h"
#include "url/gurl.h"

namespace enterprise_connectors {

// Implementation of the WinNetworkFetcher interface.
class WinNetworkFetcherImpl : public WinNetworkFetcher {
 public:
  WinNetworkFetcherImpl(const GURL& url,
                        const std::string& body,
                        base::flat_map<std::string, std::string> headers);

  ~WinNetworkFetcherImpl() override;

  // WinNetworkFetcher:
  void Fetch(FetchCompletedCallback callback) override;

 private:
  GURL url_;
  std::string body_;
  base::flat_map<std::string, std::string> headers_;
  scoped_refptr<winhttp::SharedHInternet> winhttp_session_;
  scoped_refptr<winhttp::NetworkFetcher> winhttp_network_fetcher_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_FETCHER_WIN_NETWORK_FETCHER_IMPL_H_
