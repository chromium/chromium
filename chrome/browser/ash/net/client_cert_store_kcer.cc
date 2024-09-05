// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/client_cert_store_kcer.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "ash/components/kcer/client_cert_identity_kcer.h"
#include "ash/components/kcer/kcer.h"
#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/ash/net/client_cert_filter.h"
#include "chrome/browser/certificate_provider/certificate_provider.h"
#include "net/ssl/client_cert_store_nss.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"

namespace ash {
namespace {
net::ClientCertIdentityList FilterCertsOnWorkerThread(
    scoped_refptr<const net::SSLCertRequestInfo> request,
    net::ClientCertIdentityList client_certs) {
  net::ClientCertStoreNSS::FilterCertsOnWorkerThread(&client_certs, *request);
  return client_certs;
}
}  // namespace

//==============================================================================

ClientCertStoreKcer::ClientCertStoreKcer(
    std::unique_ptr<chromeos::CertificateProvider> cert_provider,
    base::WeakPtr<kcer::Kcer> kcer)
    : cert_provider_(std::move(cert_provider)), kcer_(std::move(kcer)) {}

ClientCertStoreKcer::~ClientCertStoreKcer() {}

void ClientCertStoreKcer::GetClientCerts(
    scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
    ClientCertListCallback callback) {
  auto get_kcer_certs = base::BindOnce(
      &ClientCertStoreKcer::GetKcerCerts, weak_factory_.GetWeakPtr(),
      std::move(cert_request_info), std::move(callback));

  if (cert_provider_) {
    cert_provider_->GetCertificates(std::move(get_kcer_certs));
  } else {
    std::move(get_kcer_certs).Run(net::ClientCertIdentityList());
  }
}

void ClientCertStoreKcer::GetKcerCerts(
    scoped_refptr<const net::SSLCertRequestInfo> request,
    ClientCertListCallback callback,
    net::ClientCertIdentityList additional_certs) {
  if (!kcer_) {
    return GotAllCerts(std::move(request), std::move(callback),
                       std::move(additional_certs));
  }

  // Fetch all tokens that are available in the current context.
  kcer_->GetAvailableTokens(base::BindOnce(
      &ClientCertStoreKcer::GotKcerTokens, weak_factory_.GetWeakPtr(),
      std::move(request), std::move(callback), std::move(additional_certs)));
}

void ClientCertStoreKcer::GotKcerTokens(
    scoped_refptr<const net::SSLCertRequestInfo> request,
    ClientCertListCallback callback,
    net::ClientCertIdentityList additional_certs,
    base::flat_set<kcer::Token> tokens) {
  if (!kcer_) {
    return GotAllCerts(std::move(request), std::move(callback),
                       std::move(additional_certs));
  }

  kcer_->ListCerts(
      std::move(tokens),
      base::BindOnce(&ClientCertStoreKcer::GotKcerCerts,
                     weak_factory_.GetWeakPtr(), std::move(request),
                     std::move(callback), std::move(additional_certs)));
}

void ClientCertStoreKcer::GotKcerCerts(
    scoped_refptr<const net::SSLCertRequestInfo> request,
    ClientCertListCallback callback,
    net::ClientCertIdentityList additional_certs,
    std::vector<scoped_refptr<const kcer::Cert>> kcer_certs,
    base::flat_map<kcer::Token, kcer::Error> kcer_errors) {
  if (!kcer_) {
    return GotAllCerts(std::move(request), std::move(callback),
                       std::move(additional_certs));
  }

  for (auto& [k, v] : kcer_errors) {
    // Log errors, if any, and continue as usual with the remaining certs.
    LOG(ERROR) << base::StringPrintf(
        "Failed to get certs from token:%d, error:%d", static_cast<uint32_t>(k),
        static_cast<uint32_t>(v));
  }

  additional_certs.reserve(additional_certs.size() + kcer_certs.size());
  for (scoped_refptr<const kcer::Cert>& cert : kcer_certs) {
    if (!cert || !cert->GetX509Cert()) {
      // Probably shouldn't happen, but double check just in case.
      continue;
    }

    additional_certs.push_back(
        std::make_unique<kcer::ClientCertIdentityKcer>(kcer_, std::move(cert)));
  }

  return GotAllCerts(std::move(request), std::move(callback),
                     std::move(additional_certs));
}

void ClientCertStoreKcer::GotAllCerts(
    scoped_refptr<const net::SSLCertRequestInfo> request,
    ClientCertListCallback callback,
    net::ClientCertIdentityList certs) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&FilterCertsOnWorkerThread, std::move(request),
                     std::move(certs)),
      base::BindOnce(&ClientCertStoreKcer::ReturnClientCerts,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ClientCertStoreKcer::ReturnClientCerts(
    ClientCertListCallback callback,
    net::ClientCertIdentityList identities) {
  std::move(callback).Run(std::move(identities));
}

}  // namespace ash
