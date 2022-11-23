// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_LOGIN_CLIENT_CERT_USAGE_OBSERVER_H_
#define CHROME_BROWSER_ASH_LOGIN_LOGIN_CLIENT_CERT_USAGE_OBSERVER_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "chrome/browser/certificate_provider/certificate_provider_service.h"
#include "chromeos/ash/components/login/auth/public/challenge_response_key.h"
#include "net/cert/x509_certificate.h"

namespace ash {

// Observes and remembers the extension-provided client certificates that were
// used to successfully sign data on the login screen.
class LoginClientCertUsageObserver final
    : public chromeos::CertificateProviderService::Observer {
 public:
  LoginClientCertUsageObserver();

  LoginClientCertUsageObserver(const LoginClientCertUsageObserver&) = delete;
  LoginClientCertUsageObserver& operator=(const LoginClientCertUsageObserver&) =
      delete;

  ~LoginClientCertUsageObserver() override;

  // Returns whether at least one certificate was used.
  bool ClientCertsWereUsed() const;
  // Returns whether exactly one unique certificate was used, and, if so,
  // writes this certificate to `cert` and appends the signature algorithms
  // supported by its provider into `signature_algorithms`.
  // The certificate was signed by the extension `extension_id`.
  bool GetOnlyUsedClientCert(
      scoped_refptr<net::X509Certificate>* cert,
      std::vector<ChallengeResponseKey::SignatureAlgorithm>*
          signature_algorithms,
      std::string* extension_id) const;

 private:
  // CertificateProviderService::Observer:
  void OnSignCompleted(const scoped_refptr<net::X509Certificate>& certificate,
                       const std::string& extension_id) override;

  // How many times the client certificate, used on the login screen, has
  // changed.
  int used_cert_count_ = 0;
  // One of the client certificates that was used on the login screen.
  scoped_refptr<net::X509Certificate> used_cert_;
  // The extension that signed `used_cert_`.
  std::string used_extension_id_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_LOGIN_CLIENT_CERT_USAGE_OBSERVER_H_
