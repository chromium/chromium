// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/client_cert_store_chromeos.h"

#include <cert.h>
#include <algorithm>
#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider.h"
#include "chrome/browser/chromeos/net/client_cert_filter_chromeos.h"
#include "crypto/nss_crypto_module_delegate.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"

namespace chromeos {

ClientCertStoreChromeOS::ClientCertStoreChromeOS(
    std::unique_ptr<CertificateProvider> cert_provider,
    bool use_system_slot,
    const std::string& username_hash,
    const PasswordDelegateFactory& password_delegate_factory)
    : cert_provider_(std::move(cert_provider)),
      cert_filter_(use_system_slot, username_hash) {}

ClientCertStoreChromeOS::~ClientCertStoreChromeOS() {}

void ClientCertStoreChromeOS::GetClientCerts(
    const net::SSLCertRequestInfo& cert_request_info,
    ClientCertListCallback callback) {
  // Caller is responsible for keeping the ClientCertStore alive until the
  // callback is run.
  base::OnceCallback<void(net::ClientCertIdentityList)>
      get_platform_certs_and_filter = base::BindOnce(
          &ClientCertStoreChromeOS::GotAdditionalCerts, base::Unretained(this),
          base::Unretained(&cert_request_info), std::move(callback));

  base::OnceClosure get_additional_certs_and_continue;
  if (cert_provider_) {
    get_additional_certs_and_continue =
        base::BindOnce(&CertificateProvider::GetCertificates,
                       base::Unretained(cert_provider_.get()),
                       std::move(get_platform_certs_and_filter));
  } else {
    get_additional_certs_and_continue =
        base::BindOnce(std::move(get_platform_certs_and_filter),
                       net::ClientCertIdentityList());
  }

  auto repeating_callback = base::AdaptCallbackForRepeating(
      std::move(get_additional_certs_and_continue));
  if (cert_filter_.Init(repeating_callback))
    repeating_callback.Run();
}

void ClientCertStoreChromeOS::GotAdditionalCerts(
    const net::SSLCertRequestInfo* request,
    ClientCertListCallback callback,
    net::ClientCertIdentityList additional_certs) {
  scoped_refptr<crypto::CryptoModuleBlockingPasswordDelegate> password_delegate;
  if (!password_delegate_factory_.is_null())
    password_delegate = password_delegate_factory_.Run(request->host_and_port);
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ClientCertStoreChromeOS::GetAndFilterCertsOnWorkerThread,
                     base::Unretained(this), password_delegate,
                     base::Unretained(request), std::move(additional_certs)),
      std::move(callback));
}

net::ClientCertIdentityList
ClientCertStoreChromeOS::GetAndFilterCertsOnWorkerThread(
    scoped_refptr<crypto::CryptoModuleBlockingPasswordDelegate>
        password_delegate,
    const net::SSLCertRequestInfo* request,
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
      base::BindRepeating(&ClientCertFilterChromeOS::IsCertAllowed,
                          base::Unretained(&cert_filter_)),
      &client_certs);

  client_certs.reserve(client_certs.size() + additional_certs.size());
  for (std::unique_ptr<net::ClientCertIdentity>& cert : additional_certs)
    client_certs.push_back(std::move(cert));
  net::ClientCertStoreNSS::FilterCertsOnWorkerThread(&client_certs, *request);
  return client_certs;
}

}  // namespace chromeos
