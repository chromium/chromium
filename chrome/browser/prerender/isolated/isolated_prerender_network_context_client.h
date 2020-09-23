// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_NETWORK_CONTEXT_CLIENT_H_
#define CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_NETWORK_CONTEXT_CLIENT_H_

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/network_context.mojom.h"

// This is a NetworkContextClient that purposely does nothing so that no extra
// network traffic can occur during an Isolated Prerender, potentially causing a
// privacy leak to the user.
class IsolatedPrerenderNetworkContextClient
    : public network::mojom::NetworkContextClient {
 public:
  IsolatedPrerenderNetworkContextClient();
  ~IsolatedPrerenderNetworkContextClient() override;

  // network::mojom::NetworkContextClient implementation:
  void OnAuthRequired(
      const base::Optional<base::UnguessableToken>& window_id,
      int32_t process_id,
      int32_t routing_id,
      uint32_t request_id,
      const GURL& url,
      bool first_auth_attempt,
      const net::AuthChallengeInfo& auth_info,
      network::mojom::URLResponseHeadPtr head,
      mojo::PendingRemote<network::mojom::AuthChallengeResponder>
          auth_challenge_responder) override;
  void OnCertificateRequested(
      const base::Optional<base::UnguessableToken>& window_id,
      int32_t process_id,
      int32_t routing_id,
      uint32_t request_id,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
      mojo::PendingRemote<network::mojom::ClientCertificateResponder>
          cert_responder) override;
  void OnSSLCertificateError(int32_t process_id,
                             int32_t routing_id,
                             const GURL& url,
                             int net_error,
                             const net::SSLInfo& ssl_info,
                             bool fatal,
                             OnSSLCertificateErrorCallback response) override;
  void OnFileUploadRequested(int32_t process_id,
                             bool async,
                             const std::vector<base::FilePath>& file_paths,
                             OnFileUploadRequestedCallback callback) override;
  void OnCanSendReportingReports(
      const std::vector<url::Origin>& origins,
      OnCanSendReportingReportsCallback callback) override;
  void OnCanSendDomainReliabilityUpload(
      const GURL& origin,
      OnCanSendDomainReliabilityUploadCallback callback) override;
  void OnClearSiteData(int32_t process_id,
                       int32_t routing_id,
                       const GURL& url,
                       const std::string& header_value,
                       int load_flags,
                       OnClearSiteDataCallback callback) override;
#if defined(OS_ANDROID)
  void OnGenerateHttpNegotiateAuthToken(
      const std::string& server_auth_token,
      bool can_delegate,
      const std::string& auth_negotiate_android_account_type,
      const std::string& spn,
      OnGenerateHttpNegotiateAuthTokenCallback callback) override;
#endif
#if defined(OS_CHROMEOS)
  void OnTrustAnchorUsed() override;
#endif
};

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_NETWORK_CONTEXT_CLIENT_H_
