// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNALS_CERTIFICATE_SIGNALS_COLLECTOR_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNALS_CERTIFICATE_SIGNALS_COLLECTOR_DELEGATE_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/device_signals/core/browser/certificate_signals_collector.h"

class Profile;

namespace net {
class ClientCertStore;
}  // namespace net

namespace enterprise_signals {

// Creates the client certificate store of a profile for the
// CertificateSignalsCollector.
class CertificateSignalsCollectorDelegateImpl
    : public device_signals::CertificateSignalsCollector::Delegate {
 public:
  explicit CertificateSignalsCollectorDelegateImpl(Profile* profile);

  CertificateSignalsCollectorDelegateImpl(
      const CertificateSignalsCollectorDelegateImpl&) = delete;
  CertificateSignalsCollectorDelegateImpl& operator=(
      const CertificateSignalsCollectorDelegateImpl&) = delete;

  ~CertificateSignalsCollectorDelegateImpl() override;

  // device_signals::CertificateSignalsCollector::Delegate:
  std::unique_ptr<net::ClientCertStore> CreateClientCertStore() override;

 private:
  raw_ptr<Profile> profile_;
};

}  // namespace enterprise_signals

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNALS_CERTIFICATE_SIGNALS_COLLECTOR_DELEGATE_IMPL_H_
