// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/client_cert_store_ash.h"

#include <cert.h>
#include <algorithm>
#include <iterator>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/ash/net/client_cert_filter.h"
#include "chrome/browser/certificate_provider/certificate_provider.h"
#include "crypto/nss_crypto_module_delegate.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"

namespace ash {

ClientCertStoreAsh::ClientCertStoreAsh(
    std::unique_ptr<chromeos::CertificateProvider> cert_provider,
    bool use_system_slot,
    const std::string& username_hash,
    const PasswordDelegateFactory& password_delegate_factory)
    : cert_provider_(std::move(cert_provider)),
      cert_filter_(base::MakeRefCounted<ClientCertFilter>(use_system_slot,
                                                          username_hash)) {}

ClientCertStoreAsh::~ClientCertStoreAsh() {}

void ClientCertStoreAsh::GetClientCerts(
    scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
    ClientCertListCallback callback) {
  base::OnceCallback<void(net::ClientCertIdentityList)>
      get_platform_certs_and_filter = base::BindOnce(
          &ClientCertStoreAsh::GotAdditionalCerts, weak_factory_.GetWeakPtr(),
          std::move(cert_request_info), std::move(callback));

  auto split_callback = base::SplitOnceCallback(base::BindOnce(
      &ClientCertStoreAsh::GetAdditionalCertsAndContinue,
      weak_factory_.GetWeakPtr(), std::move(get_platform_certs_and_filter)));

  if (cert_filter_->Init(std::move(split_callback.first))) {
    std::move(split_callback.second).Run();
  }
}

void ClientCertStoreAsh::GotAdditionalCerts(
    scoped_refptr<const net::SSLCertRequestInfo> request,
    ClientCertListCallback callback,
    net::ClientCertIdentityList additional_certs) {
  scoped_refptr<crypto::CryptoModuleBlockingPasswordDelegate> password_delegate;
  if (!password_delegate_factory_.is_null())
    password_delegate = password_delegate_factory_.Run(request->host_and_port);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ClientCertStoreAsh::GetAndFilterCertsOnWorkerThread,
                     cert_filter_, password_delegate, std::move(request),
                     std::move(additional_certs)),
      base::BindOnce(&ClientCertStoreAsh::OnClientCertsResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ClientCertStoreAsh::GetAdditionalCertsAndContinue(
    base::OnceCallback<void(net::ClientCertIdentityList)> callback) {
  if (cert_provider_) {
    cert_provider_->GetCertificates(
        base::BindOnce(&ClientCertStoreAsh::OnClientCertsResponse,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    std::move(callback).Run(net::ClientCertIdentityList());
  }
}

void ClientCertStoreAsh::OnClientCertsResponse(
    ClientCertListCallback callback,
    net::ClientCertIdentityList identities) {
  std::move(callback).Run(std::move(identities));
}

// static
net::ClientCertIdentityList ClientCertStoreAsh::GetAndFilterCertsOnWorkerThread(
    scoped_refptr<ClientCertFilter> cert_filter,
    scoped_refptr<crypto::CryptoModuleBlockingPasswordDelegate>
        password_delegate,
    scoped_refptr<const net::SSLCertRequestInfo> request,
    net::ClientCertIdentityList additional_certs) {
  // This method may acquire the NSS lock or reenter this code via extension
  // hooks (such as smart card UI). To ensure threads are not starved or
  // deadlocked, the base::ScopedBlockingCall below increments the thread pool
  // capacity if this method takes too much time to run.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  net::ClientCertIdentityList client_certs;
  net::ClientCertStoreNSS::GetPlatformCertsOnWorkerThread(
      std::move(password_delegate),
      // This use of base::Unretained is safe because the callback is called
      // synchronously.
      base::BindRepeating(&ClientCertFilter::IsCertAllowed,
                          base::Unretained(cert_filter.get())),
      &client_certs);

  client_certs.reserve(client_certs.size() + additional_certs.size());
  for (std::unique_ptr<net::ClientCertIdentity>& cert : additional_certs)
    client_certs.push_back(std::move(cert));
  net::ClientCertStoreNSS::FilterCertsOnWorkerThread(&client_certs, *request);
  return client_certs;
}

}  // namespace ash
