// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_H_

#include <vector>

#include "base/macros.h"
#include "net/ssl/client_cert_identity.h"

namespace chromeos {

class CertificateProvider {
 public:
  CertificateProvider() {}
  virtual ~CertificateProvider() {}

  virtual void GetCertificates(
      base::OnceCallback<void(net::ClientCertIdentityList)> callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CertificateProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_H_
