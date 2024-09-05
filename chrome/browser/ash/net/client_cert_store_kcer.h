// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_CLIENT_CERT_STORE_KCER_H_
#define CHROME_BROWSER_ASH_NET_CLIENT_CERT_STORE_KCER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/components/kcer/kcer.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "net/ssl/client_cert_store.h"

namespace chromeos {
class CertificateProvider;
}

namespace ash {

class ClientCertStoreKcer : public net::ClientCertStore {
 public:
  // This ClientCertStore will return client certs from `kcer` (which might have
  // access to user and/or device certs depending on how it was created). `kcer`
  // will only return certs that are allowed to be used in the current context.
  // It will additionally return certificates provided by `cert_provider`.
  ClientCertStoreKcer(
      std::unique_ptr<chromeos::CertificateProvider> cert_provider,
      base::WeakPtr<kcer::Kcer> kcer);

  ClientCertStoreKcer(const ClientCertStoreKcer&) = delete;
  ClientCertStoreKcer& operator=(const ClientCertStoreKcer&) = delete;

  ~ClientCertStoreKcer() override;

  // net::ClientCertStore:
  void GetClientCerts(
      scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
      ClientCertListCallback callback) override;

 private:
  void GetKcerCerts(scoped_refptr<const net::SSLCertRequestInfo> request,
                    ClientCertListCallback callback,
                    net::ClientCertIdentityList additional_certs);
  void GotKcerTokens(scoped_refptr<const net::SSLCertRequestInfo> request,
                     ClientCertListCallback callback,
                     net::ClientCertIdentityList additional_certs,
                     base::flat_set<kcer::Token> tokens);
  void GotKcerCerts(scoped_refptr<const net::SSLCertRequestInfo> request,
                    ClientCertListCallback callback,
                    net::ClientCertIdentityList additional_certs,
                    std::vector<scoped_refptr<const kcer::Cert>> kcer_certs,
                    base::flat_map<kcer::Token, kcer::Error> kcer_errors);
  void GotAllCerts(scoped_refptr<const net::SSLCertRequestInfo> request,
                   ClientCertListCallback callback,
                   net::ClientCertIdentityList certs);
  void ReturnClientCerts(ClientCertListCallback callback,
                         net::ClientCertIdentityList identities);

  std::unique_ptr<chromeos::CertificateProvider> cert_provider_;
  // The correct instance of Kcer for this ClientCertStoreKcer (either related
  // to a Profile or device-wide). The Profile-bound Kcer might get invalidated
  // if ClientCertStoreKcer outlives the Profile. This is probably not expected,
  // but it's hard to verify and enforce, so `kcer_` should be checked before
  // usage for safety.
  base::WeakPtr<kcer::Kcer> kcer_;

  base::WeakPtrFactory<ClientCertStoreKcer> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_CLIENT_CERT_STORE_KCER_H_
