// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_CLIENT_CERT_STORE_LACROS_H_
#define CHROME_BROWSER_LACROS_CLIENT_CERT_STORE_LACROS_H_

#include "base/callback_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/client_cert_store_nss.h"

class CertDbInitializer;

// Provides client certs for Lacros-Chrome.
// Client certificates may not be available during initialization, as
// Lacros-Chrome needs to get configuration data from Ash-Chrome for the
// user. ClientCertStoreLacros will queue requests for client certificates
// until CertDbInitializer has completed initializing, and then dispatch
// requests to the underlying ClientCertStore.
class ClientCertStoreLacros final : public net::ClientCertStore {
 public:
  ClientCertStoreLacros(CertDbInitializer* cert_db_initializer,
                        std::unique_ptr<net::ClientCertStore> underlying_store);
  ClientCertStoreLacros(const ClientCertStoreLacros&) = delete;
  ClientCertStoreLacros& operator=(ClientCertStoreLacros&) = delete;
  ~ClientCertStoreLacros() override;

  // net::ClientCertStore
  void GetClientCerts(const net::SSLCertRequestInfo& cert_request_info,
                      ClientCertListCallback callback) override;

 private:
  using RequestQueue =
      std::vector<std::pair<scoped_refptr<const net::SSLCertRequestInfo>,
                            ClientCertListCallback>>;

  void WaitForCertDb();
  void OnCertDbReady(bool is_cert_db_ready);

  bool are_certs_loaded_ = false;
  CertDbInitializer* cert_db_initializer_ = nullptr;
  base::CallbackListSubscription wait_subscription_;
  RequestQueue pending_requests_;

  std::unique_ptr<net::ClientCertStore> underlying_store_;

  base::WeakPtrFactory<ClientCertStoreLacros> weak_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_CLIENT_CERT_STORE_LACROS_H_
