// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_network_context_client.h"

#include <memory>

#include "build/chromeos_buildflags.h"
#include "mojo/public/cpp/bindings/remote.h"

PrefetchProxyNetworkContextClient::PrefetchProxyNetworkContextClient() =
    default;
PrefetchProxyNetworkContextClient::~PrefetchProxyNetworkContextClient() =
    default;

void PrefetchProxyNetworkContextClient::OnFileUploadRequested(
    int32_t process_id,
    bool async,
    const std::vector<base::FilePath>& file_paths,
    OnFileUploadRequestedCallback callback) {
  std::move(callback).Run(net::ERR_ACCESS_DENIED, std::vector<base::File>());
}

void PrefetchProxyNetworkContextClient::OnCanSendReportingReports(
    const std::vector<url::Origin>& origins,
    OnCanSendReportingReportsCallback callback) {
  std::move(callback).Run(std::vector<url::Origin>());
}

void PrefetchProxyNetworkContextClient::OnCanSendDomainReliabilityUpload(
    const GURL& origin,
    OnCanSendDomainReliabilityUploadCallback callback) {
  std::move(callback).Run(false);
}

#if defined(OS_ANDROID)
void PrefetchProxyNetworkContextClient::OnGenerateHttpNegotiateAuthToken(
    const std::string& server_auth_token,
    bool can_delegate,
    const std::string& auth_negotiate_android_account_type,
    const std::string& spn,
    OnGenerateHttpNegotiateAuthTokenCallback callback) {
  std::move(callback).Run(net::ERR_FAILED, server_auth_token);
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
void PrefetchProxyNetworkContextClient::OnTrustAnchorUsed() {}
#endif

void PrefetchProxyNetworkContextClient::OnTrustTokenIssuanceDivertedToSystem(
    network::mojom::FulfillTrustTokenIssuanceRequestPtr request,
    OnTrustTokenIssuanceDivertedToSystemCallback callback) {
  auto response = network::mojom::FulfillTrustTokenIssuanceAnswer::New();
  response->status =
      network::mojom::FulfillTrustTokenIssuanceAnswer::Status::kNotFound;
  std::move(callback).Run(std::move(response));
}
