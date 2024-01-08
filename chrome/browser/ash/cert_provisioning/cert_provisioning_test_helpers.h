// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_TEST_HELPERS_H_
#define CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_TEST_HELPERS_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/platform_keys/mock_platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace user_manager {
class User;
}  // namespace user_manager

namespace ash {
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
      const std::optional<CertProfileId>& cert_profile_id,
      chromeos::platform_keys::Status status,
      base::Time not_valid_before,
      base::Time not_valid_after);

  // Simplified version of AddCert(). The certificate is not expired and has
  // |cert_profile_id|.
  scoped_refptr<net::X509Certificate> AddCert(
      CertScope cert_scope,
      const std::optional<CertProfileId>& cert_profile_id);

  // Simplified version of AddCert(). The certificate is not expired, but fails
  // to retrieve |cert_profile_id|.
  scoped_refptr<net::X509Certificate> AddCert(
      CertScope cert_scope,
      const std::optional<CertProfileId>& cert_profile_id,
      chromeos::platform_keys::Status status);

  void ClearCerts();
  const net::CertificateList& GetCerts() const;

 private:
  void GetCertificates(chromeos::platform_keys::TokenId token_id,
                       platform_keys::GetCertificatesCallback callback);

  raw_ptr<platform_keys::MockPlatformKeysService> platform_keys_service_ =
      nullptr;
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
  user_manager::TypedScopedUserManager<FakeChromeUserManager>
      fake_user_manager_{std::make_unique<FakeChromeUserManager>()};
  raw_ptr<TestingProfile> testing_profile_ = nullptr;
  raw_ptr<user_manager::User> user_ = nullptr;
};

}  // namespace cert_provisioning
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_TEST_HELPERS_H_
