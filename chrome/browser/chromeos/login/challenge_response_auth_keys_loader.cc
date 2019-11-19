// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/challenge_response_auth_keys_loader.h"

#include <cstdint>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/login/auth/challenge_response/cert_utils.h"
#include "chromeos/login/auth/challenge_response/known_user_pref_utils.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"

namespace chromeos {

namespace {

// Loads the persistently stored information about the challenge-response keys
// that can be used for authenticating the user.
std::vector<std::string> LoadStoredChallengeResponseSpkiKeysForUser(
    const AccountId& account_id) {
  const base::Value known_user_value =
      user_manager::known_user::GetChallengeResponseKeys(account_id);
  std::vector<std::string> spki_items;
  if (!DeserializeChallengeResponseKeysFromKnownUser(known_user_value,
                                                     &spki_items)) {
    return {};
  }
  return spki_items;
}

// Returns the certificate provider service that should be used for querying the
// currently available cryptographic keys.
// The sign-in profile is used since it's where the needed extensions are
// installed (e.g., for the smart card based login they are force-installed via
// the DeviceLoginScreenExtensions admin policy).
CertificateProviderService* GetCertificateProviderService() {
  return CertificateProviderServiceFactory::GetForBrowserContext(
      ProfileHelper::GetSigninProfile());
}

// Maps from the TLS 1.3 SignatureScheme values into the challenge-response key
// algorithm list.
std::vector<ChallengeResponseKey::SignatureAlgorithm> MakeAlgorithmListFromSsl(
    const std::vector<uint16_t>& ssl_algorithms) {
  std::vector<ChallengeResponseKey::SignatureAlgorithm>
      challenge_response_algorithms;
  for (auto ssl_algorithm : ssl_algorithms) {
    base::Optional<ChallengeResponseKey::SignatureAlgorithm> algorithm =
        GetChallengeResponseKeyAlgorithmFromSsl(ssl_algorithm);
    if (algorithm)
      challenge_response_algorithms.push_back(*algorithm);
  }
  return challenge_response_algorithms;
}

}  // namespace

// static
bool ChallengeResponseAuthKeysLoader::CanAuthenticateUser(
    const AccountId& account_id) {
  return !LoadStoredChallengeResponseSpkiKeysForUser(account_id).empty();
}

ChallengeResponseAuthKeysLoader::ChallengeResponseAuthKeysLoader() = default;

ChallengeResponseAuthKeysLoader::~ChallengeResponseAuthKeysLoader() = default;

void ChallengeResponseAuthKeysLoader::LoadAvailableKeys(
    const AccountId& account_id,
    LoadAvailableKeysCallback callback) {
  // Load the list of public keys of the cryptographic keys that can be used for
  // authenticating the user.
  std::vector<std::string> suitable_public_key_spki_items =
      LoadStoredChallengeResponseSpkiKeysForUser(account_id);
  if (suitable_public_key_spki_items.empty()) {
    // This user's profile doesn't support challenge-response authentication.
    std::move(callback).Run({} /* challenge_response_keys */);
    return;
  }

  // Asynchronously poll all certificate providers to get the list of currently
  // available cryptographic keys.
  std::unique_ptr<CertificateProvider> cert_provider =
      GetCertificateProviderService()->CreateCertificateProvider();
  cert_provider->GetCertificates(base::BindOnce(
      &ChallengeResponseAuthKeysLoader::ContinueLoadAvailableKeysWithCerts,
      weak_ptr_factory_.GetWeakPtr(), account_id,
      std::move(suitable_public_key_spki_items), std::move(callback)));
}

void ChallengeResponseAuthKeysLoader::ContinueLoadAvailableKeysWithCerts(
    const AccountId& account_id,
    const std::vector<std::string>& suitable_public_key_spki_items,
    LoadAvailableKeysCallback callback,
    net::ClientCertIdentityList /* cert_identities */) {
  CertificateProviderService* const cert_provider_service =
      GetCertificateProviderService();
  std::vector<ChallengeResponseKey> filtered_keys;
  // Filter those of the currently available cryptographic keys that can be used
  // for authenticating the user. Also fill out for the selected keys the
  // currently available cryptographic signature algorithms.
  for (const auto& suitable_spki : suitable_public_key_spki_items) {
    std::vector<uint16_t> supported_ssl_algorithms;
    cert_provider_service->GetSupportedAlgorithmsBySpki(
        suitable_spki, &supported_ssl_algorithms);
    if (supported_ssl_algorithms.empty()) {
      // This key is not currently exposed by any certificate provider or,
      // potentially, is exposed but without supporting any signature algorithm.
      continue;
    }
    std::vector<ChallengeResponseKey::SignatureAlgorithm> supported_algorithms =
        MakeAlgorithmListFromSsl(supported_ssl_algorithms);
    if (supported_algorithms.empty()) {
      // This currently available key doesn't support any of the algorithms that
      // are supported by the challenge-response user authentication.
      continue;
    }
    ChallengeResponseKey filtered_key;
    filtered_key.set_public_key_spki_der(suitable_spki);
    filtered_key.set_signature_algorithms(supported_algorithms);
    filtered_keys.push_back(filtered_key);
  }
  std::move(callback).Run(std::move(filtered_keys));
}

}  // namespace chromeos
