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
#include "components/device_signals/core/common/signals_features.h"
#include "net/cert/cert_database.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"

namespace enterprise_signals {

class ProfileNetworkContextServiceWrapperImpl
    : public ProfileNetworkContextServiceWrapper {
 public:
  explicit ProfileNetworkContextServiceWrapperImpl(
      ProfileNetworkContextService* profile_network_context_service)
      : profile_network_context_service_(profile_network_context_service) {
    CHECK(profile_network_context_service_);
  }

  ~ProfileNetworkContextServiceWrapperImpl() override = default;

  // ProfileNetworkContextServiceWrapper:
  std::unique_ptr<net::ClientCertStore> CreateClientCertStore() override {
    return profile_network_context_service_->CreateClientCertStore();
  }

  void FlushCachedClientCertIfNeeded(
      const net::HostPortPair& host,
      const scoped_refptr<net::X509Certificate>& certificate) override {
    profile_network_context_service_->FlushCachedClientCertIfNeeded(
        host, certificate);
  }

 private:
  raw_ptr<ProfileNetworkContextService> profile_network_context_service_;
};

ClientCertificateFetcher::ClientCertificateFetcher(
    std::unique_ptr<ProfileNetworkContextServiceWrapper>
        profile_network_context_service_wrapper,
    Profile* profile)
    : profile_network_context_service_wrapper_(
          std::move(profile_network_context_service_wrapper)),
      profile_(profile) {
  CHECK(profile_network_context_service_wrapper_);
  CHECK(profile_);
  client_cert_store_ =
      profile_network_context_service_wrapper_->CreateClientCertStore();
}

ClientCertificateFetcher::~ClientCertificateFetcher() = default;

// static
std::unique_ptr<ClientCertificateFetcher> ClientCertificateFetcher::Create(
    content::BrowserContext* browser_context) {
  auto* profile_network_context_service =
      ProfileNetworkContextServiceFactory::GetForContext(browser_context);
  if (!profile_network_context_service) {
    return nullptr;
  }

  return std::make_unique<ClientCertificateFetcher>(
      std::make_unique<ProfileNetworkContextServiceWrapperImpl>(
          profile_network_context_service),
      Profile::FromBrowserContext(browser_context));
}

void ClientCertificateFetcher::FetchAutoSelectedCertificateForUrl(
    const GURL& url,
    FetchAutoSelectedCertificateForUrlCallback callback) {
  if (!url.is_valid() || !client_cert_store_) {
    std::move(callback).Run(nullptr);
    return;
  }

  fetch_callback_ = std::move(callback);
  cert_request_info_ = base::MakeRefCounted<net::SSLCertRequestInfo>();
  client_cert_store_->GetClientCerts(
      cert_request_info_,
      base::BindOnce(&ClientCertificateFetcher::OnGetClientCertsComplete,
                     weak_ptr_factory_.GetWeakPtr(), url));
}

void ClientCertificateFetcher::OnGetClientCertsComplete(
    const GURL& url,
    net::ClientCertIdentityList client_certs) {
  net::ClientCertIdentityList matching_certificates, nonmatching_certificates;
  enterprise_util::AutoSelectCertificates(
      profile_, url, std::move(client_certs), &matching_certificates,
      &nonmatching_certificates);

  std::unique_ptr<net::ClientCertIdentity> selected_cert;
  if (!matching_certificates.empty()) {
    // In case of multiple matching certificates simply take the first one,
    // given the lack of other criteria here.
    selected_cert = std::move(matching_certificates[0]);
  }

  // Make sure the network stack's cached client certificate matches with the
  // one that is about to be returned (or not).
  if (features::IsClearClientCertsOnExtensionReportEnabled()) {
    profile_network_context_service_wrapper_->FlushCachedClientCertIfNeeded(
        net::HostPortPair::FromURL(url),
        selected_cert ? selected_cert->certificate() : nullptr);
  }

  std::move(fetch_callback_).Run(std::move(selected_cert));
}

}  // namespace enterprise_signals
