// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNALS_CLIENT_CERTIFICATE_FETCHER_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNALS_CLIENT_CERTIFICATE_FETCHER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/host_port_pair.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity.h"
#include "url/gurl.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace net {
class ClientCertStore;
class SSLCertRequestInfo;
}  // namespace net

namespace enterprise_signals {

// Class that wraps a ProfileNetworkContextService instance to allow easier
// mocking of that instance in unit tests.
class ProfileNetworkContextServiceWrapper {
 public:
  virtual ~ProfileNetworkContextServiceWrapper() = default;

  virtual std::unique_ptr<net::ClientCertStore> CreateClientCertStore() = 0;
  virtual void FlushCachedClientCertIfNeeded(
      const net::HostPortPair& host,
      const scoped_refptr<net::X509Certificate>& certificate) = 0;
};

class ClientCertificateFetcher {
 public:
  ClientCertificateFetcher(std::unique_ptr<ProfileNetworkContextServiceWrapper>
                               profile_network_context_service_wrapper,
                           Profile* profile);
  ~ClientCertificateFetcher();

  static std::unique_ptr<ClientCertificateFetcher> Create(
      content::BrowserContext* browser_context);

  using FetchAutoSelectedCertificateForUrlCallback =
      base::OnceCallback<void(std::unique_ptr<net::ClientCertIdentity>)>;

  void FetchAutoSelectedCertificateForUrl(
      const GURL& url,
      FetchAutoSelectedCertificateForUrlCallback callback);

 private:
  void OnGetClientCertsComplete(const GURL& url,
                                net::ClientCertIdentityList client_certs);

  std::unique_ptr<ProfileNetworkContextServiceWrapper>
      profile_network_context_service_wrapper_;
  std::unique_ptr<net::ClientCertStore> client_cert_store_;
  raw_ptr<Profile> profile_;

  FetchAutoSelectedCertificateForUrlCallback fetch_callback_;
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;

  base::WeakPtrFactory<ClientCertificateFetcher> weak_ptr_factory_{this};
};

}  // namespace enterprise_signals

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNALS_CLIENT_CERTIFICATE_FETCHER_H_
