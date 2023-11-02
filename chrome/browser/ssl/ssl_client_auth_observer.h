// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_OBSERVER_H_
#define CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_OBSERVER_H_

#include <memory>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"

namespace net {
class SSLCertRequestInfo;
class SSLPrivateKey;
class X509Certificate;
}

namespace content {
class BrowserContext;
class ClientCertificateDelegate;
}

// SSLClientAuthObserver is a base class that wraps a
// ClientCertificateDelegate. It links client certificate selection dialogs
// attached to the same BrowserContext. When CertificateSelected is called via
// one of them, the rest simulate the same action.
class SSLClientAuthObserver {
 public:
  SSLClientAuthObserver(
      const content::BrowserContext* browser_context,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_request_info,
      std::unique_ptr<content::ClientCertificateDelegate> delegate);

  SSLClientAuthObserver(const SSLClientAuthObserver&) = delete;
  SSLClientAuthObserver& operator=(const SSLClientAuthObserver&) = delete;

  virtual ~SSLClientAuthObserver();

  // UI should implement this to close the dialog.
  virtual void OnCertSelectedByNotification() = 0;

  // Continues the request with a certificate. Can also call with nullptr to
  // continue with no certificate. Derived classes must use this instead of
  // caching the delegate and calling it directly.
  void CertificateSelected(net::X509Certificate* cert,
                           net::SSLPrivateKey* private_key);

  // Cancels the certificate selection and aborts the request.
  void CancelCertificateSelection();

  // Begins observing notifications from other SSLClientAuthObserver instances.
  // If another instance chooses a cert for a matching SSLCertRequestInfo, we
  // will also use the same cert and OnCertSelectedByNotification will be called
  // so that the cert selection UI can be closed.
  //
  // The caller must call CertificateSelected(), CancelCertificateSelection(),
  // or StopObserving() before the SSLClientAuthObserver is destroyed.
  void StartObserving();

  // Stops observing notifications.  We will no longer act on client auth
  // notifications.
  void StopObserving();

  net::SSLCertRequestInfo* cert_request_info() const {
    return cert_request_info_.get();
  }

 private:
  void CertificateSelectedWithOtherObserver(
      const content::BrowserContext* browser_context,
      net::SSLCertRequestInfo* cert_request_info,
      net::X509Certificate* certificate,
      net::SSLPrivateKey* private_key);

  static std::set<SSLClientAuthObserver*>& GetActiveObservers();

  raw_ptr<const content::BrowserContext> browser_context_;
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;
  std::unique_ptr<content::ClientCertificateDelegate> delegate_;
};

#endif  // CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_OBSERVER_H_
