// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/client_certificates/profile_context_delegate.h"

#include "base/check.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "net/cert/x509_certificate.h"

namespace client_certificates {

ProfileContextDelegate::ProfileContextDelegate(
    ProfileNetworkContextService* profile_network_context_service)
    : profile_network_context_service_(profile_network_context_service) {
  CHECK(profile_network_context_service_);
}

ProfileContextDelegate::~ProfileContextDelegate() = default;

void ProfileContextDelegate::OnClientCertificateDeleted(
    scoped_refptr<net::X509Certificate> certificate) {
  profile_network_context_service_->FlushMatchingCachedClientCert(certificate);
}

}  // namespace client_certificates
