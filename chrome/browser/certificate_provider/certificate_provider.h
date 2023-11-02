// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_H_
#define CHROME_BROWSER_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_H_

#include "net/ssl/client_cert_identity.h"

namespace chromeos {

class CertificateProvider {
 public:
  CertificateProvider() {}
  CertificateProvider(const CertificateProvider&) = delete;
  CertificateProvider& operator=(const CertificateProvider&) = delete;
  virtual ~CertificateProvider() {}

  virtual void GetCertificates(
      base::OnceCallback<void(net::ClientCertIdentityList)> callback) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_H_
