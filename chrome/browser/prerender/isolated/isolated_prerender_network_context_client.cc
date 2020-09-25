// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_network_context_client.h"

#include <memory>

#include "mojo/public/cpp/bindings/remote.h"

IsolatedPrerenderNetworkContextClient::IsolatedPrerenderNetworkContextClient() =
    default;
IsolatedPrerenderNetworkContextClient::
    ~IsolatedPrerenderNetworkContextClient() = default;

void IsolatedPrerenderNetworkContextClient::OnAuthRequired(
    const base::Optional<base::UnguessableToken>& window_id,
    int32_t process_id,
    int32_t routing_id,
    uint32_t request_id,
    const GURL& url,
    bool first_auth_attempt,
    const net::AuthChallengeInfo& auth_info,
    network::mojom::URLResponseHeadPtr head,
    mojo::PendingRemote<network::mojom::AuthChallengeResponder>
        auth_challenge_responder) {
  mojo::Remote<network::mojom::AuthChallengeResponder>
      auth_challenge_responder_remote(std::move(auth_challenge_responder));
  auth_challenge_responder_remote->OnAuthCredentials(base::nullopt);
}

void IsolatedPrerenderNetworkContextClient::OnCertificateRequested(
    const base::Optional<base::UnguessableToken>& window_id,
    int32_t process_id,
    int32_t routing_id,
    uint32_t request_id,
    const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
    mojo::PendingRemote<network::mojom::ClientCertificateResponder>
        cert_responder_remote) {
  mojo::Remote<network::mojom::ClientCertificateResponder> cert_responder(
      std::move(cert_responder_remote));
  cert_responder->CancelRequest();
}

void IsolatedPrerenderNetworkContextClient::OnSSLCertificateError(
    int32_t process_id,
    int32_t routing_id,
    const GURL& url,
    int net_error,
    const net::SSLInfo& ssl_info,
    bool fatal,
    OnSSLCertificateErrorCallback response) {
  std::move(response).Run(net::ERR_ABORTED);
}

void IsolatedPrerenderNetworkContextClient::OnFileUploadRequested(
    int32_t process_id,
    bool async,
    const std::vector<base::FilePath>& file_paths,
    OnFileUploadRequestedCallback callback) {
  std::move(callback).Run(net::ERR_ACCESS_DENIED, std::vector<base::File>());
}

void IsolatedPrerenderNetworkContextClient::OnCanSendReportingReports(
    const std::vector<url::Origin>& origins,
    OnCanSendReportingReportsCallback callback) {
  std::move(callback).Run(std::vector<url::Origin>());
}

void IsolatedPrerenderNetworkContextClient::OnCanSendDomainReliabilityUpload(
    const GURL& origin,
    OnCanSendDomainReliabilityUploadCallback callback) {
  std::move(callback).Run(false);
}

void IsolatedPrerenderNetworkContextClient::OnClearSiteData(
    int32_t process_id,
    int32_t routing_id,
    const GURL& url,
    const std::string& header_value,
    int load_flags,
    OnClearSiteDataCallback callback) {
  std::move(callback).Run();
}

#if defined(OS_ANDROID)
void IsolatedPrerenderNetworkContextClient::OnGenerateHttpNegotiateAuthToken(
    const std::string& server_auth_token,
    bool can_delegate,
    const std::string& auth_negotiate_android_account_type,
    const std::string& spn,
    OnGenerateHttpNegotiateAuthTokenCallback callback) {
  std::move(callback).Run(net::ERR_FAILED, server_auth_token);
}
#endif

#if defined(OS_CHROMEOS)
void IsolatedPrerenderNetworkContextClient::OnTrustAnchorUsed() {}
#endif
