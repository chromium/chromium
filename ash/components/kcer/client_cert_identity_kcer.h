// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_KCER_CLIENT_CERT_IDENTITY_KCER_H_
#define ASH_COMPONENTS_KCER_CLIENT_CERT_IDENTITY_KCER_H_

#include "ash/components/kcer/kcer.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/ssl_private_key.h"

namespace kcer {

class COMPONENT_EXPORT(KCER) ClientCertIdentityKcer
    : public net::ClientCertIdentity {
 public:
  ClientCertIdentityKcer(base::WeakPtr<Kcer> kcer,
                         scoped_refptr<const Cert> cert);
  ~ClientCertIdentityKcer() override;

  // Implements net::ClientCertIdentity.
  void AcquirePrivateKey(
      base::OnceCallback<void(scoped_refptr<net::SSLPrivateKey>)>
          private_key_callback) override;

 private:
  void OnGotKeyInfo(base::OnceCallback<void(scoped_refptr<net::SSLPrivateKey>)>
                        private_key_callback,
                    base::expected<KeyInfo, Error> key_info);

  // The Kcer instance that should be used to work with `cert_`.
  base::WeakPtr<Kcer> kcer_;
  scoped_refptr<const Cert> cert_;
  base::WeakPtrFactory<ClientCertIdentityKcer> weak_factory_{this};
};

}  // namespace kcer

#endif  // ASH_COMPONENTS_KCER_CLIENT_CERT_IDENTITY_KCER_H_
