// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/kcer/kcer.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "ash/components/kcer/kcer_impl.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "net/cert/x509_certificate.h"

namespace kcer {

//==================== PublicKey ===============================================

PublicKey::PublicKey(Token token,
                     Pkcs11Id pkcs11_id,
                     PublicKeySpki pub_key_spki)
    : token_(token),
      pkcs11_id_(std::move(pkcs11_id)),
      pub_key_spki_(std::move(pub_key_spki)) {}

PublicKey::PublicKey(const PublicKey&) = default;
PublicKey& PublicKey::operator=(const PublicKey&) = default;
PublicKey::PublicKey(PublicKey&&) = default;
PublicKey& PublicKey::operator=(PublicKey&&) = default;
PublicKey::~PublicKey() = default;

bool PublicKey::operator==(const PublicKey& other) const {
  return ((token_ == other.token_) && (pkcs11_id_ == other.pkcs11_id_) &&
          (pub_key_spki_ == other.pub_key_spki_));
}
bool PublicKey::operator!=(const PublicKey& other) const {
  return !operator==(other);
}

//==================== KeyInfo =================================================

KeyInfo::KeyInfo() = default;
KeyInfo::~KeyInfo() = default;
KeyInfo::KeyInfo(const KeyInfo&) = default;
KeyInfo& KeyInfo::operator=(const KeyInfo&) = default;
KeyInfo::KeyInfo(KeyInfo&&) = default;
KeyInfo& KeyInfo::operator=(KeyInfo&&) = default;

//==================== Cert ====================================================

Cert::Cert(Token token,
           Pkcs11Id pkcs11_id,
           std::string nickname,
           scoped_refptr<net::X509Certificate> x509_cert)
    : token_(token),
      pkcs11_id_(std::move(pkcs11_id)),
      nickname_(std::move(nickname)),
      x509_cert_(std::move(x509_cert)) {}
Cert::~Cert() = default;

//==================== PrivateKeyHandle ========================================

PrivateKeyHandle::PrivateKeyHandle(const PublicKey& public_key)
    : token_(public_key.GetToken()), pkcs11_id_(public_key.GetPkcs11Id()) {}
PrivateKeyHandle::PrivateKeyHandle(const Cert& cert)
    : token_(cert.GetToken()), pkcs11_id_(cert.GetPkcs11Id()) {}
PrivateKeyHandle::PrivateKeyHandle(PublicKeySpki pub_key_spki)
    : pub_key_spki_(std::move(pub_key_spki)) {}
PrivateKeyHandle::PrivateKeyHandle(Token token, PublicKeySpki pub_key_spki)
    : token_(token), pub_key_spki_(std::move(pub_key_spki)) {}
PrivateKeyHandle::PrivateKeyHandle(Token token, PrivateKeyHandle&& other)
    : token_(token),
      pkcs11_id_(std::move(other.pkcs11_id_)),
      pub_key_spki_(std::move(other.pub_key_spki_)) {
  CHECK(!other.token_.has_value());
}

PrivateKeyHandle::~PrivateKeyHandle() = default;
PrivateKeyHandle::PrivateKeyHandle(const PrivateKeyHandle&) = default;
PrivateKeyHandle& PrivateKeyHandle::operator=(const PrivateKeyHandle&) =
    default;
PrivateKeyHandle::PrivateKeyHandle(PrivateKeyHandle&&) = default;
PrivateKeyHandle& PrivateKeyHandle::operator=(PrivateKeyHandle&&) = default;

}  // namespace kcer
