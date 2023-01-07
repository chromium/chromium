// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/client_certificate_fetcher.h"

#include <memory>
#include <utility>

#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"

namespace enterprise_signals {

ClientCertificateFetcher::ClientCertificateFetcher(
    std::unique_ptr<net::ClientCertStore> client_cert_store,
    content::BrowserContext* browser_context)
    : client_cert_store_(std::move(client_cert_store)),
      profile_(Profile::FromBrowserContext(browser_context)) {}

ClientCertificateFetcher::~ClientCertificateFetcher() = default;

// static
std::unique_ptr<ClientCertificateFetcher> ClientCertificateFetcher::Create(
    content::BrowserContext* browser_context) {
  return std::make_unique<ClientCertificateFetcher>(
      ProfileNetworkContextServiceFactory::GetForContext(browser_context)
          ->CreateClientCertStore(),
      browser_context);
}

void ClientCertificateFetcher::FetchAutoSelectedCertificateForUrl(
    const GURL& url,
    FetchAutoSelectedCertificateForUrlCallback callback) {
  if (!client_cert_store_) {
    std::move(callback).Run(nullptr);
    return;
  }

  requesting_url_ = url;

  fetch_callback_ = std::move(callback);
  cert_request_info_ = base::MakeRefCounted<net::SSLCertRequestInfo>();
  client_cert_store_->GetClientCerts(
      *cert_request_info_,
      base::BindOnce(&ClientCertificateFetcher::OnGetClientCertsComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ClientCertificateFetcher::OnGetClientCertsComplete(
    net::ClientCertIdentityList client_certs) {
  net::ClientCertIdentityList matching_certificates, nonmatching_certificates;
  chrome::enterprise_util::AutoSelectCertificates(
      profile_, requesting_url_, std::move(client_certs),
      &matching_certificates, &nonmatching_certificates);

  std::unique_ptr<net::ClientCertIdentity> selected_cert;
  if (!matching_certificates.empty()) {
    // In case of multiple matching certificates simply take the first one,
    // given the lack of other criteria here.
    selected_cert = std::move(matching_certificates[0]);
  }

  std::move(fetch_callback_).Run(std::move(selected_cert));
}

}  // namespace enterprise_signals
