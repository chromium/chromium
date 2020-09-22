// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_TEST_HELPERS_H_
#define CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_TEST_HELPERS_H_

#include "base/optional.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/platform_keys/mock_platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace user_manager {
class User;
}

namespace chromeos {
namespace cert_provisioning {

//================ CertificateHelperForTesting =================================

// Allows to add certificate to a fake storage with assigned CertProfileId-s.
// Redirects PlatformKeysService::GetCertificate calls to itself and return all
// stored certificates as a result.
struct CertificateHelperForTesting {
 public:
  explicit CertificateHelperForTesting(
      platform_keys::MockPlatformKeysService* platform_keys_service);
  ~CertificateHelperForTesting();

  // Generates and adds a certificate to internal fake certificate storage.
  // Returns refpointer to the generated certificate. If |status| is an error
  // status, an attempt to retrieve |cert_profile_id| via
  // PlatformKeysService::GetAttributeForKey() will fail with |status|.
  // |not_valid_before|, |not_valid_after| configure validity period of the
  // certificate.
  scoped_refptr<net::X509Certificate> AddCert(
      CertScope cert_scope,
      const base::Optional<CertProfileId>& cert_profile_id,
      platform_keys::Status status,
      base::Time not_valid_before,
      base::Time not_valid_after);

  // Simplified version of AddCert(). The certificate is not expired and has
  // |cert_profile_id|.
  scoped_refptr<net::X509Certificate> AddCert(
      CertScope cert_scope,
      const base::Optional<CertProfileId>& cert_profile_id);

  // Simplified version of AddCert(). The certificate is not expired, but fails
  // to retrieve |cert_profile_id|.
  scoped_refptr<net::X509Certificate> AddCert(
      CertScope cert_scope,
      const base::Optional<CertProfileId>& cert_profile_id,
      platform_keys::Status status);

  void ClearCerts();
  const net::CertificateList& GetCerts() const;

 private:
  void GetCertificates(platform_keys::TokenId token_id,
                       platform_keys::GetCertificatesCallback callback);

  platform_keys::MockPlatformKeysService* platform_keys_service_ = nullptr;
  scoped_refptr<net::X509Certificate> template_cert_;
  net::CertificateList cert_list_;
};

//================ ProfileHelperForTesting =====================================

class ProfileHelperForTesting {
 public:
  // Equivalent to ProfileHelperForTesting(/*user_is_affiliated=*/false)
  ProfileHelperForTesting();
  explicit ProfileHelperForTesting(bool user_is_affiliated);
  ProfileHelperForTesting(const ProfileHelperForTesting&) = delete;
  ProfileHelperForTesting& operator=(const ProfileHelperForTesting&) = delete;
  ~ProfileHelperForTesting();

  Profile* GetProfile() const;
  user_manager::User* GetUser() const;

 private:
  void Init(bool user_is_affiliated);

  TestingProfileManager testing_profile_manager_;
  FakeChromeUserManager fake_user_manager_;
  TestingProfile* testing_profile_ = nullptr;
  user_manager::User* user_ = nullptr;
};

//================ SpyingFakeCryptohomeClient ==================================

class SpyingFakeCryptohomeClient : public FakeCryptohomeClient {
 public:
  SpyingFakeCryptohomeClient();
  SpyingFakeCryptohomeClient(const SpyingFakeCryptohomeClient&) = delete;
  SpyingFakeCryptohomeClient& operator=(const SpyingFakeCryptohomeClient&) =
      delete;
  ~SpyingFakeCryptohomeClient() override;

  void TpmAttestationDeleteKey(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::string& key_prefix,
      DBusMethodCallback<bool> callback) override;

  void TpmAttestationDeleteKeysByPrefix(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::string& key_prefix,
      DBusMethodCallback<bool> callback) override;

  MOCK_METHOD(void,
              OnTpmAttestationDeleteKey,
              (attestation::AttestationKeyType key_type,
               const std::string& key_prefix));

  MOCK_METHOD(void,
              OnTpmAttestationDeleteKeysByPrefix,
              (attestation::AttestationKeyType key_type,
               const std::string& key_prefix));
};

}  // namespace cert_provisioning
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_TEST_HELPERS_H_
