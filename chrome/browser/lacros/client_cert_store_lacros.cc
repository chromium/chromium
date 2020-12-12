// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/client_cert_store_lacros.h"

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/lacros/cert_db_initializer.h"
#include "net/ssl/client_cert_store_nss.h"
#include "net/ssl/ssl_cert_request_info.h"

ClientCertStoreLacros::ClientCertStoreLacros(
    CertDbInitializer* cert_db_initializer,
    std::unique_ptr<net::ClientCertStore> underlying_store)
    : cert_db_initializer_(cert_db_initializer),
      underlying_store_(std::move(underlying_store)) {
  DCHECK(underlying_store_);
  DCHECK(cert_db_initializer_);

  WaitForCertDb();
}

ClientCertStoreLacros::~ClientCertStoreLacros() = default;

void ClientCertStoreLacros::GetClientCerts(
    const net::SSLCertRequestInfo& cert_request_info,
    ClientCertListCallback callback) {
  if (!are_certs_loaded_) {
    pending_requests_.push_back(std::make_pair(
        WrapRefCounted(&cert_request_info), std::move(callback)));
    return;
  }

  underlying_store_->GetClientCerts(cert_request_info, std::move(callback));
}

void ClientCertStoreLacros::WaitForCertDb() {
  wait_subscription_ = cert_db_initializer_->WaitUntilReady(base::BindOnce(
      &ClientCertStoreLacros::OnCertDbReady, weak_factory_.GetWeakPtr()));
}

void ClientCertStoreLacros::OnCertDbReady(bool /*is_cert_db_ready*/) {
  // Ignore the initialization result. Even if it failed, some certificates
  // could be accessible.

  // Ensure any new requests (e.g. that result from invoking the
  // callbacks) aren't queued.
  are_certs_loaded_ = true;

  // Move the pending requests to the stack, since `this` may
  // be deleted by the last request callback.
  decltype(pending_requests_) local_requests;
  std::swap(pending_requests_, local_requests);

  // Dispatch all the queued requests.
  for (auto& request : local_requests) {
    underlying_store_->GetClientCerts(*request.first,
                                      std::move(request.second));
  }
}
