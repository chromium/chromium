// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_CLIENT_CERT_USAGE_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_CLIENT_CERT_USAGE_OBSERVER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chromeos/login/auth/challenge_response_key.h"
#include "net/cert/x509_certificate.h"

namespace chromeos {

// Observes and remembers the extension-provided client certificates that were
// used to successfully sign data on the login screen.
class LoginClientCertUsageObserver final
    : public CertificateProviderService::Observer {
 public:
  LoginClientCertUsageObserver();
  ~LoginClientCertUsageObserver() override;

  // Returns whether at least one certificate was used.
  bool ClientCertsWereUsed() const;
  // Returns whether exactly one unique certificate was used, and, if so,
  // writes this certificate to |cert| and appends the signature algorithms
  // supported by its provider into |signature_algorithms|.
  bool GetOnlyUsedClientCert(
      scoped_refptr<net::X509Certificate>* cert,
      std::vector<ChallengeResponseKey::SignatureAlgorithm>*
          signature_algorithms) const;

 private:
  // CertificateProviderService::Observer:
  void OnSignCompleted(
      const scoped_refptr<net::X509Certificate>& certificate) override;

  // How many times the client certificate, used on the login screen, has
  // changed.
  int used_cert_count_ = 0;
  // One of the client certificates that was used on the login screen.
  scoped_refptr<net::X509Certificate> used_cert_;

  DISALLOW_COPY_AND_ASSIGN(LoginClientCertUsageObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_CLIENT_CERT_USAGE_OBSERVER_H_
