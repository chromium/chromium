// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/client_cert_store_ash.h"

#include <cert.h>
#include <algorithm>
#include <iterator>
#include <utility>

// TMP
#include "base/base64.h"
#include "base/debug/stack_trace.h"
#include "base/debug/task_trace.h"
#include "base/logging.h"
#define HERE() LOG(ERROR) << " === QCERT " << __FUNCTION__ << " "
// #define HERE() LAZY_STREAM(LOG_STREAM(ERROR), /*is_on=*/false)

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

// TMP QCERT
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/net/ssl_private_key_kcer.h"
#include "chromeos/components/kcer/kcer.h"
#include "net/cert/x509_util_nss.h"
#include "net/ssl/client_cert_store_nss.h"

namespace ash {
namespace {
// TMP QCERT double check that I don't need ClientCertFilter here
// like in GetAndFilterCertsOnWorkerThread . Shouldn't need it because a kcer
// instance is already bound to a user/profile and it's handled whether the
// system slot should be used or not.
net::ClientCertIdentityList FilterCertsOnWorkerThread(
    scoped_refptr<const net::SSLCertRequestInfo> request,
    net::ClientCertIdentityList client_certs) {
  net::ClientCertStoreNSS::FilterCertsOnWorkerThread(&client_certs, *request);
  return client_certs;
}
}  // namespace

ClientCertIdentityKcer::ClientCertIdentityKcer(
    base::WeakPtr<kcer::Kcer> kcer,
    scoped_refptr<const kcer::Cert> kcer_cert)
    : ClientCertIdentity(kcer_cert->GetX509Cert()),
      kcer_(std::move(kcer)),
      kcer_cert_(std::move(kcer_cert)) {}
ClientCertIdentityKcer::~ClientCertIdentityKcer() = default;

void ClientCertIdentityKcer::AcquirePrivateKey(
    base::OnceCallback<void(scoped_refptr<net::SSLPrivateKey>)>
        private_key_callback) {
  kcer_->GetKeyInfo(kcer::PrivateKeyHandle(*kcer_cert_),
                    base::BindOnce(&ClientCertIdentityKcer::OnGotKeyInfo,
                                   weak_factory_.GetWeakPtr(),
                                   std::move(private_key_callback)));
}

void ClientCertIdentityKcer::OnGotKeyInfo(
    base::OnceCallback<void(scoped_refptr<net::SSLPrivateKey>)>
        private_key_callback,
    base::expected<kcer::KeyInfo, kcer::Error> key_info) {
  auto key = base::MakeRefCounted<net::SSLPrivateKeyKcer>(
      kcer_, kcer_cert_, KcerKeyTypeToEvp(key_info->key_type),
      key_info->supported_signing_schemes);
  std::move(private_key_callback).Run(std::move(key));
}

int ClientCertIdentityKcer::KcerKeyTypeToEvp(kcer::KeyType key_type) {
  switch (key_type) {
    case kcer::KeyType::kRsa:
      HERE() << "ok rsa";
      return EVP_PKEY_RSA;
    case kcer::KeyType::kEcc:
      HERE() << "ok ecc";
      return EVP_PKEY_EC;
  }
}

//==============================================================================

ClientCertStoreAsh::ClientCertStoreAsh(
    std::unique_ptr<chromeos::CertificateProvider> cert_provider,
    bool use_system_slot,
    const std::string& username_hash,
    base::WeakPtr<kcer::Kcer> kcer,
    const PasswordDelegateFactory& password_delegate_factory)
    : cert_provider_(std::move(cert_provider)),
      cert_filter_(base::MakeRefCounted<ClientCertFilter>(use_system_slot,
                                                          username_hash)),
      use_system_token_(use_system_slot),
      kcer_(std::move(kcer)) {}

ClientCertStoreAsh::~ClientCertStoreAsh() {}

void ClientCertStoreAsh::GetClientCerts(
    scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
    ClientCertListCallback callback) {
  HERE() << "ok start";

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
  HERE() << "ok end";
}

void ClientCertStoreAsh::GotAdditionalCerts(
    scoped_refptr<const net::SSLCertRequestInfo> request,
    ClientCertListCallback callback,
    net::ClientCertIdentityList additional_certs) {
  HERE() << "ok start";
  scoped_refptr<crypto::CryptoModuleBlockingPasswordDelegate> password_delegate;
  if (!password_delegate_factory_.is_null())
    password_delegate = password_delegate_factory_.Run(request->host_and_port);

  // TMP QCERT check lifetime of the ClientCertStore, maybe I don't
  // need to check kcer_
  if (ash::features::CacheClientCertQueries() && !kcer_) {
    HERE() << "FAIL this is not expected to happen";
    std::move(callback).Run(net::ClientCertIdentityList());
    return;
  }
  if (ash::features::CacheClientCertQueries() && kcer_) {
    auto got_kcer_tokens = base::BindOnce(
        &ClientCertStoreAsh::GotKcerTokens, weak_factory_.GetWeakPtr(),
        std::move(request), std::move(callback), std::move(additional_certs));

    if (use_system_token_) {
      HERE() << "ok optimized all tokens";
      // Fetch all available tokens and use all of them.
      kcer_->GetAvailableTokens(std::move(got_kcer_tokens));
    } else {
      HERE() << "ok optimized user token";
      // Just use the user token.
      std::move(got_kcer_tokens).Run({kcer::Token::kUser});
    }
    HERE() << "ok end 1";
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ClientCertStoreAsh::GetAndFilterCertsOnWorkerThread,
                     cert_filter_, password_delegate, std::move(request),
                     std::move(additional_certs)),
      base::BindOnce(&ClientCertStoreAsh::OnClientCertsResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
  HERE() << "ok end 2";
}

void ClientCertStoreAsh::GotKcerTokens(
    scoped_refptr<const net::SSLCertRequestInfo> request,
    ClientCertListCallback callback,
    net::ClientCertIdentityList additional_certs,
    base::flat_set<kcer::Token> tokens) {
  HERE() << "ok";
  kcer_->ListCerts(
      std::move(tokens),
      base::BindOnce(&ClientCertStoreAsh::GotKcerCerts,
                     weak_factory_.GetWeakPtr(), std::move(request),
                     std::move(callback), std::move(additional_certs)));
}

void ClientCertStoreAsh::GotKcerCerts(
    scoped_refptr<const net::SSLCertRequestInfo> request,
    ClientCertListCallback callback,
    net::ClientCertIdentityList additional_certs,
    std::vector<scoped_refptr<const kcer::Cert>> kcer_certs,
    base::flat_map<kcer::Token, kcer::Error> kcer_errors) {
  HERE() << "ok begin";
  if (!kcer_errors.empty() || !kcer_) {
    HERE() << "FAIL ";
    if (!kcer_errors.empty()) {
      for (auto& [k, v] : kcer_errors) {
        HERE() << "FAIL " << static_cast<int>(v);
      }
    }
    if (!kcer_) {
      HERE() << "FAIL kcer is null";
    }
    std::move(callback).Run(net::ClientCertIdentityList());
    return;
  }

  additional_certs.reserve(additional_certs.size() + kcer_certs.size());
  for (scoped_refptr<const kcer::Cert>& cert : kcer_certs) {
    additional_certs.push_back(
        std::make_unique<ClientCertIdentityKcer>(kcer_, std::move(cert)));
  }

  HERE() << "ok end TOTAL IDENTITIES " << additional_certs.size();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&FilterCertsOnWorkerThread, std::move(request),
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
  HERE() << "ok start";
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
  HERE() << "ok end";
  return client_certs;
}

}  // namespace ash
