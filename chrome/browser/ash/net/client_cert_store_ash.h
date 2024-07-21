// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_CLIENT_CERT_STORE_ASH_H_
#define CHROME_BROWSER_ASH_NET_CLIENT_CERT_STORE_ASH_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/net/client_cert_filter.h"
#include "net/ssl/client_cert_store_nss.h"

namespace chromeos {
class CertificateProvider;
}

namespace ash {

// This class is being deprecated and replaced with ClientCertStoreKcer.
class ClientCertStoreAsh : public net::ClientCertStore {
 public:
  using PasswordDelegateFactory =
      net::ClientCertStoreNSS::PasswordDelegateFactory;

  // This ClientCertStore will return client certs from the public
  // and private slot of the user with |username_hash| and with the system slot
  // if |use_system_slot| is true. If |username_hash| is empty, no public and no
  // private slot will be used. It will additionally return certificates
  // provided by |cert_provider|.
  ClientCertStoreAsh(
      std::unique_ptr<chromeos::CertificateProvider> cert_provider,
      bool use_system_slot,
      const std::string& username_hash,
      const PasswordDelegateFactory& password_delegate_factory);

  ClientCertStoreAsh(const ClientCertStoreAsh&) = delete;
  ClientCertStoreAsh& operator=(const ClientCertStoreAsh&) = delete;

  ~ClientCertStoreAsh() override;

  // net::ClientCertStore:
  void GetClientCerts(
      scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
      ClientCertListCallback callback) override;

 private:
  void GotAdditionalCerts(scoped_refptr<const net::SSLCertRequestInfo> request,
                          ClientCertListCallback callback,
                          net::ClientCertIdentityList additional_certs);
  void GetAdditionalCertsAndContinue(
      base::OnceCallback<void(net::ClientCertIdentityList)> callback);

  static net::ClientCertIdentityList GetAndFilterCertsOnWorkerThread(
      scoped_refptr<ClientCertFilter> cert_filter,
      scoped_refptr<crypto::CryptoModuleBlockingPasswordDelegate>
          password_delegate,
      scoped_refptr<const net::SSLCertRequestInfo> request,
      net::ClientCertIdentityList additional_certs);

  void OnClientCertsResponse(ClientCertListCallback callback,
                             net::ClientCertIdentityList identities);

  std::unique_ptr<chromeos::CertificateProvider> cert_provider_;
  scoped_refptr<ClientCertFilter> cert_filter_;

  // The factory for creating the delegate for requesting a password to a
  // PKCS#11 token. May be null.
  PasswordDelegateFactory password_delegate_factory_;

  base::WeakPtrFactory<ClientCertStoreAsh> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_CLIENT_CERT_STORE_ASH_H_
