// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/login_client_cert_usage_observer.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "base/logging.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/login/auth/challenge_response/cert_utils.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_util.h"

namespace ash {
namespace {

chromeos::CertificateProviderService* GetCertificateProviderService() {
  Profile* signin_profile = ProfileHelper::GetSigninProfile();
  return chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
      signin_profile);
}

bool ObtainSignatureAlgorithms(
    const net::X509Certificate& cert,
    std::vector<ChallengeResponseKey::SignatureAlgorithm>*
        signature_algorithms) {
  auto* certificate_provider_service = GetCertificateProviderService();
  std::string_view spki;
  if (!net::asn1::ExtractSPKIFromDERCert(
          net::x509_util::CryptoBufferAsStringPiece(cert.cert_buffer()),
          &spki)) {
    return false;
  }
  std::vector<uint16_t> ssl_algorithms;
  std::string extension_id;
  if (!certificate_provider_service->LookUpSpki(
          std::string(spki), &ssl_algorithms, &extension_id)) {
    return false;
  }
  signature_algorithms->clear();
  for (auto ssl_algorithm : ssl_algorithms) {
    std::optional<ChallengeResponseKey::SignatureAlgorithm> algorithm =
        GetChallengeResponseKeyAlgorithmFromSsl(ssl_algorithm);
    if (algorithm)
      signature_algorithms->push_back(*algorithm);
  }
  return !signature_algorithms->empty();
}

}  // namespace

LoginClientCertUsageObserver::LoginClientCertUsageObserver() {
  GetCertificateProviderService()->AddObserver(this);
}

LoginClientCertUsageObserver::~LoginClientCertUsageObserver() {
  GetCertificateProviderService()->RemoveObserver(this);
}

bool LoginClientCertUsageObserver::ClientCertsWereUsed() const {
  return used_cert_count_ > 0;
}

bool LoginClientCertUsageObserver::GetOnlyUsedClientCert(
    scoped_refptr<net::X509Certificate>* cert,
    std::vector<ChallengeResponseKey::SignatureAlgorithm>* signature_algorithms,
    std::string* extension_id) const {
  if (!used_cert_count_)
    return false;
  if (used_cert_count_ > 1) {
    LOG(ERROR)
        << "Failed to choose the client certificate for offline user "
           "authentication, since more than one client certificate was used";
    return false;
  }
  DCHECK(used_cert_);
  if (!ObtainSignatureAlgorithms(*used_cert_, signature_algorithms))
    return false;
  *cert = used_cert_;
  *extension_id = used_extension_id_;
  return true;
}

void LoginClientCertUsageObserver::OnSignCompleted(
    const scoped_refptr<net::X509Certificate>& certificate,
    const std::string& extension_id) {
  if (!used_cert_ || !used_cert_->EqualsExcludingChain(certificate.get()))
    ++used_cert_count_;
  used_cert_ = certificate;
  used_extension_id_ = extension_id;
}

}  // namespace ash
