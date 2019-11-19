// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_CLIENT_CERT_STORE_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_NET_CLIENT_CERT_STORE_CHROMEOS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/net/client_cert_filter_chromeos.h"
#include "net/ssl/client_cert_store_nss.h"

namespace chromeos {

class CertificateProvider;

class ClientCertStoreChromeOS : public net::ClientCertStore {
 public:
  using PasswordDelegateFactory =
      net::ClientCertStoreNSS::PasswordDelegateFactory;

  // This ClientCertStore will return client certs from the public
  // and private slot of the user with |username_hash| and with the system slot
  // if |use_system_slot| is true. If |username_hash| is empty, no public and no
  // private slot will be used. It will additionally return certificates
  // provided by |cert_provider|.
  ClientCertStoreChromeOS(
      std::unique_ptr<CertificateProvider> cert_provider,
      bool use_system_slot,
      const std::string& username_hash,
      const PasswordDelegateFactory& password_delegate_factory);
  ~ClientCertStoreChromeOS() override;

  // net::ClientCertStore:
  void GetClientCerts(const net::SSLCertRequestInfo& cert_request_info,
                      ClientCertListCallback callback) override;

 private:
  void GotAdditionalCerts(const net::SSLCertRequestInfo* request,
                          ClientCertListCallback callback,
                          net::ClientCertIdentityList additional_certs);

  net::ClientCertIdentityList GetAndFilterCertsOnWorkerThread(
      scoped_refptr<crypto::CryptoModuleBlockingPasswordDelegate>
          password_delegate,
      const net::SSLCertRequestInfo* request,
      net::ClientCertIdentityList additional_certs);

  std::unique_ptr<CertificateProvider> cert_provider_;
  ClientCertFilterChromeOS cert_filter_;

  // The factory for creating the delegate for requesting a password to a
  // PKCS#11 token. May be null.
  PasswordDelegateFactory password_delegate_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClientCertStoreChromeOS);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_CLIENT_CERT_STORE_CHROMEOS_H_
