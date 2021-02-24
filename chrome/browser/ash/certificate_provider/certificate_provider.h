// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_H_
#define CHROME_BROWSER_ASH_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_H_

#include <vector>

#include "net/ssl/client_cert_identity.h"

namespace ash {

class CertificateProvider {
 public:
  CertificateProvider() {}
  CertificateProvider(const CertificateProvider&) = delete;
  CertificateProvider& operator=(const CertificateProvider&) = delete;
  virtual ~CertificateProvider() {}

  virtual void GetCertificates(
      base::OnceCallback<void(net::ClientCertIdentityList)> callback) = 0;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when Chrome OS code migration is
// done.
namespace chromeos {
using ::ash::CertificateProvider;
}

#endif  // CHROME_BROWSER_ASH_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_H_
