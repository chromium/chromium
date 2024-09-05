// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/kcer/client_cert_identity_kcer.h"

#include "ash/components/kcer/kcer.h"
#include "ash/components/kcer/ssl_private_key_kcer.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/ssl_private_key.h"

namespace kcer {

ClientCertIdentityKcer::ClientCertIdentityKcer(base::WeakPtr<Kcer> kcer,
                                               scoped_refptr<const Cert> cert)
    : ClientCertIdentity(cert ? cert->GetX509Cert() : nullptr),
      kcer_(std::move(kcer)),
      cert_(std::move(cert)) {}
ClientCertIdentityKcer::~ClientCertIdentityKcer() = default;

void ClientCertIdentityKcer::AcquirePrivateKey(
    base::OnceCallback<void(scoped_refptr<net::SSLPrivateKey>)>
        private_key_callback) {
  if (!kcer_ || !cert_) {
    return std::move(private_key_callback).Run(nullptr);
  }

  kcer_->GetKeyInfo(PrivateKeyHandle(*cert_),
                    base::BindOnce(&ClientCertIdentityKcer::OnGotKeyInfo,
                                   weak_factory_.GetWeakPtr(),
                                   std::move(private_key_callback)));
}

void ClientCertIdentityKcer::OnGotKeyInfo(
    base::OnceCallback<void(scoped_refptr<net::SSLPrivateKey>)>
        private_key_callback,
    base::expected<KeyInfo, Error> key_info) {
  if (!kcer_) {
    return std::move(private_key_callback).Run(nullptr);
  }

  if (!key_info.has_value()) {
    LOG(ERROR) << "Failed to acquire a private key, error: "
               << static_cast<int>(key_info.error());
    return std::move(private_key_callback).Run(nullptr);
  }

  auto key = base::MakeRefCounted<SSLPrivateKeyKcer>(
      kcer_, cert_, key_info->key_type, key_info->supported_signing_schemes);
  return std::move(private_key_callback).Run(std::move(key));
}

}  // namespace kcer
