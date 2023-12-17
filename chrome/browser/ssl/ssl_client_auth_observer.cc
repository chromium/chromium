// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_client_auth_observer.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"

using content::BrowserThread;

SSLClientAuthObserver::SSLClientAuthObserver(
    const content::BrowserContext* browser_context,
    const scoped_refptr<net::SSLCertRequestInfo>& cert_request_info,
    std::unique_ptr<content::ClientCertificateDelegate> delegate)
    : browser_context_(browser_context),
      cert_request_info_(cert_request_info),
      delegate_(std::move(delegate)) {
  DCHECK(delegate_);
}

SSLClientAuthObserver::~SSLClientAuthObserver() {
  // The caller is required to explicitly stop observing, but call
  // StopObserving() anyway to avoid a dangling pointer. (StopObserving() is
  // idempotent, so it may be called multiple times.)
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(0u, GetActiveObservers().count(this));
  StopObserving();
}

void SSLClientAuthObserver::CertificateSelected(
    net::X509Certificate* certificate,
    net::SSLPrivateKey* private_key) {
  if (!delegate_)
    return;

  // Stop listening now that the delegate has been resolved. This is also to
  // avoid getting a self-notification.
  StopObserving();

  // Iterate a copy of the observer list as other observers may remove
  // themselves from it in their callbacks.
  std::set<SSLClientAuthObserver*> observers_copy = GetActiveObservers();
  for (SSLClientAuthObserver* other_observer : observers_copy) {
    other_observer->CertificateSelectedWithOtherObserver(
        browser_context_, cert_request_info_.get(), certificate, private_key);
  }

  delegate_->ContinueWithCertificate(certificate, private_key);
  delegate_.reset();
}

void SSLClientAuthObserver::CancelCertificateSelection() {
  if (!delegate_)
    return;

  // Stop observing now that the delegate has been resolved.
  StopObserving();
  delegate_.reset();
}

void SSLClientAuthObserver::CertificateSelectedWithOtherObserver(
    const content::BrowserContext* browser_context,
    net::SSLCertRequestInfo* cert_request_info,
    net::X509Certificate* certificate,
    net::SSLPrivateKey* private_key) {
  DVLOG(1) << "SSLClientAuthObserver::Observe " << this;
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (browser_context != browser_context_)
    return;

  if (!cert_request_info->host_and_port.Equals(
          cert_request_info_->host_and_port)) {
    return;
  }

  DVLOG(1) << this << " got matching notification and selecting cert "
           << certificate;
  StopObserving();
  delegate_->ContinueWithCertificate(certificate, private_key);
  delegate_.reset();
  OnCertSelectedByNotification();
}

void SSLClientAuthObserver::StartObserving() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetActiveObservers().insert(this);
}

void SSLClientAuthObserver::StopObserving() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetActiveObservers().erase(this);
}

// static
std::set<SSLClientAuthObserver*>& SSLClientAuthObserver::GetActiveObservers() {
  static base::NoDestructor<std::set<SSLClientAuthObserver*>> observers;
  return *observers;
}
