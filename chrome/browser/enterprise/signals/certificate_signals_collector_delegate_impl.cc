// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/certificate_signals_collector_delegate_impl.h"

#include "base/check.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "net/ssl/client_cert_store.h"

namespace enterprise_signals {

CertificateSignalsCollectorDelegateImpl::
    CertificateSignalsCollectorDelegateImpl(Profile* profile)
    : profile_(profile) {
  CHECK(profile_);
}

CertificateSignalsCollectorDelegateImpl::
    ~CertificateSignalsCollectorDelegateImpl() = default;

std::unique_ptr<net::ClientCertStore>
CertificateSignalsCollectorDelegateImpl::CreateClientCertStore() {
  auto* profile_network_service =
      ProfileNetworkContextServiceFactory::GetForContext(profile_);
  return profile_network_service
             ? profile_network_service->CreateClientCertStore()
             : nullptr;
}

}  // namespace enterprise_signals
