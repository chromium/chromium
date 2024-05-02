// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/cert/client_cert_store_lacros.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/certificate_provider/certificate_provider.h"
#include "chrome/browser/lacros/cert/cert_db_initializer.h"
#include "net/ssl/client_cert_store_nss.h"
#include "net/ssl/ssl_cert_request_info.h"

ClientCertStoreLacros::ClientCertStoreLacros(
    std::unique_ptr<chromeos::CertificateProvider> cert_provider,
    CertDbInitializer* cert_db_initializer,
    std::unique_ptr<net::ClientCertStore> underlying_store)
    : cert_provider_(std::move(cert_provider)),
      cert_db_initializer_(cert_db_initializer),
      underlying_store_(std::move(underlying_store)) {
  DCHECK(underlying_store_);
  DCHECK(cert_db_initializer_);

  WaitForCertDb();
}

ClientCertStoreLacros::~ClientCertStoreLacros() = default;

void ClientCertStoreLacros::GetClientCerts(
    scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
    ClientCertListCallback callback) {
  if (!are_certs_loaded_) {
    pending_requests_.push_back(
        std::make_pair(cert_request_info, std::move(callback)));
    return;
  }

  underlying_store_->GetClientCerts(
      cert_request_info,
      base::BindOnce(&ClientCertStoreLacros::AppendAdditionalCerts,
                     weak_factory_.GetWeakPtr(), std::move(cert_request_info),
                     std::move(callback)));
}

void ClientCertStoreLacros::AppendAdditionalCerts(
    scoped_refptr<const net::SSLCertRequestInfo> request,
    ClientCertListCallback callback,
    net::ClientCertIdentityList client_certs) {
  auto get_certs_and_filter = base::BindOnce(
      &ClientCertStoreLacros::GotAdditionalCerts, weak_factory_.GetWeakPtr(),
      std::move(request), std::move(callback), std::move(client_certs));
  if (cert_provider_) {
    cert_provider_->GetCertificates(std::move(get_certs_and_filter));
  } else {
    std::move(get_certs_and_filter).Run(net::ClientCertIdentityList());
  }
}

void ClientCertStoreLacros::GotAdditionalCerts(
    scoped_refptr<const net::SSLCertRequestInfo> request,
    ClientCertListCallback callback,
    net::ClientCertIdentityList client_certs,
    net::ClientCertIdentityList additional_certs) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ClientCertStoreLacros::FilterAndJoinCertsOnWorkerThread,
                     std::move(request), std::move(client_certs),
                     std::move(additional_certs)),
      base::BindOnce(&ClientCertStoreLacros::OnClientCertsResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ClientCertStoreLacros::OnClientCertsResponse(
    ClientCertListCallback callback,
    net::ClientCertIdentityList identities) {
  std::move(callback).Run(std::move(identities));
}

// static
net::ClientCertIdentityList
ClientCertStoreLacros::FilterAndJoinCertsOnWorkerThread(
    scoped_refptr<const net::SSLCertRequestInfo> request,
    net::ClientCertIdentityList client_certs,
    net::ClientCertIdentityList additional_certs) {
  // This method may acquire the NSS lock or reenter this code via extension
  // hooks (such as smart card UI). To ensure threads are not starved or
  // deadlocked, the base::ScopedBlockingCall below increments the thread pool
  // capacity if this method takes too much time to run.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  net::ClientCertStoreNSS::FilterCertsOnWorkerThread(&additional_certs,
                                                     *request);

  int first_additional_cert_index = client_certs.size();
  client_certs.reserve(first_additional_cert_index + additional_certs.size());
  for (std::unique_ptr<net::ClientCertIdentity>& cert : additional_certs)
    client_certs.push_back(std::move(cert));
  // Ensure that the sorting persists after join
  std::inplace_merge(begin(client_certs),
                     begin(client_certs) + first_additional_cert_index,
                     end(client_certs), net::ClientCertIdentitySorter());
  return client_certs;
}

void ClientCertStoreLacros::WaitForCertDb() {
  wait_subscription_ = cert_db_initializer_->WaitUntilReady(base::BindOnce(
      &ClientCertStoreLacros::OnCertDbReady, weak_factory_.GetWeakPtr()));
}

void ClientCertStoreLacros::OnCertDbReady() {
  // Ensure any new requests (e.g. that result from invoking the
  // callbacks) aren't queued.
  are_certs_loaded_ = true;

  // Move the pending requests to the stack, since `this` may
  // be deleted by the last request callback.
  decltype(pending_requests_) local_requests;
  std::swap(pending_requests_, local_requests);

  // Dispatch all the queued requests.
  for (auto& request : local_requests) {
    GetClientCerts(request.first, std::move(request.second));
  }
}
