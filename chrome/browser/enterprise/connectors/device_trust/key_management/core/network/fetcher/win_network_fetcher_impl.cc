// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/fetcher/win_network_fetcher_impl.h"

#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "url/gurl.h"

namespace enterprise_connectors {

WinNetworkFetcherImpl::WinNetworkFetcherImpl(
    const GURL& url,
    const std::string& body,
    base::flat_map<std::string, std::string> headers)
    : url_(url), body_(body), headers_(std::move(headers)) {}

WinNetworkFetcherImpl::~WinNetworkFetcherImpl() = default;

void WinNetworkFetcherImpl::Fetch(FetchCompletedCallback callback) {
  // TODO(b/202321214): need to pass in winhttp::ProxyInfo somehow.
  // If specified use it to create an winhttp::ProxyConfiguration instance.
  // Otherwise create an winhttp::AutoProxyConfiguration instance.
  if (!winhttp_network_fetcher_) {
    auto proxy_config = base::MakeRefCounted<winhttp::ProxyConfiguration>();
    winhttp_session_ = base::MakeRefCounted<winhttp::SharedHInternet>(
        winhttp::CreateSessionHandle(
            L"DeviceTrustKeyManagement", proxy_config->access_type(),
            proxy_config->proxy(), proxy_config->proxy_bypass()));
    winhttp_network_fetcher_ = base::MakeRefCounted<winhttp::NetworkFetcher>(
        winhttp_session_, std::move(proxy_config));
  }

  winhttp_network_fetcher_->PostRequest(
      url_, body_, std::string(), headers_,
      /*fetch_started_callback=*/base::DoNothing(),
      /*fetch_progress_callback=*/base::DoNothing(),
      /*fetch_completed_callback=*/std::move(callback));
}

}  // namespace enterprise_connectors
