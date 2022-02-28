// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/win_key_network_delegate.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "components/winhttp/network_fetcher.h"
#include "components/winhttp/scoped_hinternet.h"
#include "url/gurl.h"

namespace enterprise_connectors {

WinKeyNetworkDelegate::WinKeyNetworkDelegate() = default;
WinKeyNetworkDelegate::~WinKeyNetworkDelegate() = default;

KeyNetworkDelegate::HttpResponseCode
WinKeyNetworkDelegate::SendPublicKeyToDmServerSync(const GURL& url,
                                                   const std::string& dm_token,
                                                   const std::string& body) {
  DCHECK(!response_code_.has_value());
  base::flat_map<std::string, std::string> headers;
  headers.emplace("Authorization", "GoogleDMToken token=" + dm_token);

  // TODO(b/202321214): need to pass in winhttp::ProxyInfo somehow.
  // If specified use it to create an winhttp::ProxyConfiguration instance.
  // Otherwise create an winhttp::AutoProxyConfiguration instance.
  auto proxy_config = base::MakeRefCounted<winhttp::ProxyConfiguration>();
  auto session = winhttp::CreateSessionHandle(L"DeviceTrustKeyManagement",
                                              proxy_config->access_type());
  auto fetcher = base::MakeRefCounted<winhttp::NetworkFetcher>(session.get(),
                                                               proxy_config);

  base::RunLoop run_loop;
  auto completion_callback =
      base::BindOnce(&WinKeyNetworkDelegate::FetchCompleted,
                     weak_factory_.GetWeakPtr())
          .Then(run_loop.QuitClosure());

  fetcher->PostRequest(
      url, body, std::string(), headers,
      /*fetch_started_callback=*/base::DoNothing(),
      /*fetch_progress_callback=*/base::DoNothing(),
      /*fetch_completed_callback=*/std::move(completion_callback));

  run_loop.Run();

  // Clean-up the state for future calls before returning the code.
  auto response_code = response_code_.value_or(0);
  response_code_.reset();
  return response_code;
}

void WinKeyNetworkDelegate::FetchCompleted(int response_code) {
  response_code_ = response_code;
}

}  // namespace enterprise_connectors
