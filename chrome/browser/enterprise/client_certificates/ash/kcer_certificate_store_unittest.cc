// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/client_certificates/ash/kcer_certificate_store.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/kcer/kcer.h"
#include "chromeos/ash/components/kcer/kcer_nss/test_utils.h"
#include "components/enterprise/client_certificates/core/client_identity.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "components/enterprise/client_certificates/core/store_error.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace client_certificates {

namespace {

using base::test::TestFuture;

constexpr char kSpkiKey[] = "spki";
constexpr char kTestIdentityName[] = "TestIdentity";
constexpr char kTestTempIdentityName[] = "TempTestIdentity";
constexpr char kTestFinalIdentityName[] = "FinalTestIdentity";

// Aligned with the integration-test pattern in
// chromeos/ash/components/kcer/kcer_nss/kcer_nss_unittest.cc and the sibling
// kcer_private_key{,_factory}_unittest.cc: no TPM/Kcer simulation. A real
// kcer::Kcer is wired to a real (in-memory) NSS slot via
// crypto::ScopedTestNSSDB (using a content::BrowserTaskEnvironment with a real
// IO thread, which Kcer requires). Keys are genuinely generated in NSS and
// certificates are genuinely imported, so KcerCertificateStore is exercised
// end-to-end against the real Kcer stack rather than gmock expectations.
//
// The slot is software-only, so GenerateEcKey(hardware_backed=true) succeeds
// and the store records the hardware-backed source.
class KcerCertificateStoreTest : public testing::Test {
 protected:
  void SetUp() override {
    RegisterIdentityPrefs();
    store_ = MakeStore(kcer_holder_.GetKcer());
  }

  void RegisterIdentityPrefs() {
    pref_service_.registry()->RegisterDictionaryPref(kTestIdentityName);
    pref_service_.registry()->RegisterDictionaryPref(kTestTempIdentityName);
    pref_service_.registry()->RegisterDictionaryPref(kTestFinalIdentityName);
    pref_service_.registry()->RegisterDictionaryPref(
        kManagedProfileIdentityName);
    pref_service_.registry()->RegisterDictionaryPref(
        kTemporaryManagedProfileIdentityName);
  }

  std::unique_ptr<KcerCertificateStore> MakeStore(
      base::WeakPtr<kcer::Kcer> kcer) {
    return std::make_unique<KcerCertificateStore>(
        &pref_service_, std::move(kcer),
        task_environment_.GetMainThreadTaskRunner());
  }

  // Generates a real EC key in NSS via the store under `identity_name` and
  // returns it. Also persists the identity metadata in prefs as a side effect.
  scoped_refptr<PrivateKey> CreateKey(const std::string& identity_name) {
    TestFuture<StoreErrorOr<scoped_refptr<PrivateKey>>> future;
    store_->CreatePrivateKey(identity_name, future.GetCallback());
    StoreErrorOr<scoped_refptr<PrivateKey>> result = future.Take();
    CHECK(result.has_value());
    CHECK(result.value());
    return result.value();
  }

  // Builds an X.509 certificate whose SubjectPublicKeyInfo matches `key`, so
  // Kcer's ImportX509Cert links it to the real key already in the NSS slot.
  scoped_refptr<net::X509Certificate> MakeCertForKey(const PrivateKey& key) {
    std::unique_ptr<net::CertBuilder> issuer = kcer::MakeCertIssuer();
    std::unique_ptr<net::CertBuilder> cert_builder =
        kcer::MakeCertBuilder(issuer.get(), key.GetSubjectPublicKeyInfo());
    return cert_builder->GetX509Certificate();
  }

  // Returns true if a private key with `spki` is present on the user token.
  bool KeyExistsInKcer(const std::vector<uint8_t>& spki) {
    TestFuture<base::expected<bool, kcer::Error>> future;
    kcer_holder_.GetKcer()->DoesPrivateKeyExist(
        kcer::PrivateKeyHandle(kcer::Token::kUser, kcer::PublicKeySpki(spki)),
        future.GetCallback());
    base::expected<bool, kcer::Error> result = future.Take();
    return result.has_value() && result.value();
  }

  // Generates an EC key directly via Kcer (so it is NOT tagged as a browser
  // enterprise client certificate key) and returns its SPKI.
  std::vector<uint8_t> GenerateUntaggedKeyInKcer() {
    TestFuture<base::expected<kcer::PublicKey, kcer::Error>> future;
    kcer_holder_.GetKcer()->GenerateEcKey(
        kcer::Token::kUser, kcer::EllipticCurve::kP256,
        /*hardware_backed=*/true, future.GetCallback());
    base::expected<kcer::PublicKey, kcer::Error> result = future.Take();
    CHECK(result.has_value());
    return result.value().GetSpki().value();
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      content::BrowserTaskEnvironment::REAL_IO_THREAD};

  crypto::ScopedTestNSSDB nss_db_;
  kcer::TestKcerHolder kcer_holder_{/*user_slot=*/nss_db_.slot(),
                                    /*device_slot=*/nullptr};
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<KcerCertificateStore> store_;
};

// --- CreatePrivateKey tests ---

TEST_F(KcerCertificateStoreTest, CreatePrivateKey_Success) {
  TestFuture<StoreErrorOr<scoped_refptr<PrivateKey>>> future;
  store_->CreatePrivateKey(kTestIdentityName, future.GetCallback());

  StoreErrorOr<scoped_refptr<PrivateKey>> result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value());
  // The software-only NSS test slot lets GenerateEcKey(hardware_backed=true)
  // succeed, so the store records the hardware-backed source.
  EXPECT_EQ(result.value()->GetSource(), PrivateKeySource::kChromeOsHwKey);
  EXPECT_FALSE(result.value()->GetSubjectPublicKeyInfo().empty());

  // Verify the SPKI and (hardware) key source were persisted to prefs.
  const base::DictValue& identity = pref_service_.GetDict(kTestIdentityName);
  EXPECT_TRUE(identity.FindString(kSpkiKey));
  EXPECT_EQ(identity.FindInt(kKeySource).value_or(-1),
            static_cast<int>(PrivateKeySource::kChromeOsHwKey));
}

TEST_F(KcerCertificateStoreTest, CreatePrivateKey_ConflictingIdentity) {
  // Pre-populate prefs with an existing identity under the same name.
  base::DictValue existing;
  existing.Set(kSpkiKey, "existing_spki_data");
  pref_service_.SetDict(kTestIdentityName, std::move(existing));

  TestFuture<StoreErrorOr<scoped_refptr<PrivateKey>>> future;
  store_->CreatePrivateKey(kTestIdentityName, future.GetCallback());

  StoreErrorOr<scoped_refptr<PrivateKey>> result = future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), StoreError::kConflictingIdentity);
}

TEST_F(KcerCertificateStoreTest, CreatePrivateKey_GenerationFails) {
  // A Kcer with no user token cannot generate a key: GenerateEcKey fails for
  // both the hardware and software attempts, so the factory yields no key.
  kcer::TestKcerHolder no_token_holder{/*user_slot=*/nullptr,
                                       /*device_slot=*/nullptr};
  std::unique_ptr<KcerCertificateStore> store =
      MakeStore(no_token_holder.GetKcer());

  TestFuture<StoreErrorOr<scoped_refptr<PrivateKey>>> future;
  store->CreatePrivateKey(kTestIdentityName, future.GetCallback());

  StoreErrorOr<scoped_refptr<PrivateKey>> result = future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), StoreError::kCreateKeyFailed);
}

// --- CommitCertificate tests ---

TEST_F(KcerCertificateStoreTest, CommitCertificate_Success) {
  scoped_refptr<PrivateKey> key = CreateKey(kTestIdentityName);
  scoped_refptr<net::X509Certificate> cert = MakeCertForKey(*key);
  ASSERT_TRUE(cert);

  TestFuture<std::optional<StoreError>> future;
  store_->CommitCertificate(kTestIdentityName, cert, future.GetCallback());

  EXPECT_FALSE(future.Take().has_value());
}

TEST_F(KcerCertificateStoreTest, CommitCertificate_NullCert) {
  TestFuture<std::optional<StoreError>> future;
  store_->CommitCertificate(kTestIdentityName, nullptr, future.GetCallback());

  std::optional<StoreError> error = future.Take();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(error.value(), StoreError::kInvalidCertificateInput);
}

TEST_F(KcerCertificateStoreTest, CommitCertificate_NullKcer) {
  // A store whose Kcer handle is already gone cannot import certificates.
  std::unique_ptr<KcerCertificateStore> store_with_null_kcer =
      MakeStore(base::WeakPtr<kcer::Kcer>());

  scoped_refptr<PrivateKey> key = CreateKey(kTestIdentityName);
  scoped_refptr<net::X509Certificate> cert = MakeCertForKey(*key);
  ASSERT_TRUE(cert);

  TestFuture<std::optional<StoreError>> future;
  store_with_null_kcer->CommitCertificate(kTestIdentityName, cert,
                                          future.GetCallback());

  std::optional<StoreError> error = future.Take();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(error.value(), StoreError::kSaveKeyFailed);
}

// --- CommitIdentity tests ---

TEST_F(KcerCertificateStoreTest, CommitIdentity_Success) {
  // Generate a real key under the temporary identity, then build a matching
  // cert and promote the identity to its final name.
  scoped_refptr<PrivateKey> key = CreateKey(kTestTempIdentityName);
  scoped_refptr<net::X509Certificate> cert = MakeCertForKey(*key);
  ASSERT_TRUE(cert);

  TestFuture<std::optional<StoreError>> future;
  store_->CommitIdentity(kTestTempIdentityName, kTestFinalIdentityName, cert,
                         future.GetCallback());

  EXPECT_FALSE(future.Take().has_value());

  // The temporary pref is cleared and the final pref carries the metadata.
  EXPECT_TRUE(pref_service_.GetDict(kTestTempIdentityName).empty());
  const base::DictValue& final_identity =
      pref_service_.GetDict(kTestFinalIdentityName);
  EXPECT_TRUE(final_identity.FindString(kSpkiKey));
}

TEST_F(KcerCertificateStoreTest, CommitIdentity_MissingTempIdentity) {
  scoped_refptr<PrivateKey> key = CreateKey(kTestIdentityName);
  scoped_refptr<net::X509Certificate> cert = MakeCertForKey(*key);
  ASSERT_TRUE(cert);

  TestFuture<std::optional<StoreError>> future;
  store_->CommitIdentity(kTestTempIdentityName, kTestFinalIdentityName, cert,
                         future.GetCallback());

  std::optional<StoreError> error = future.Take();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(error.value(), StoreError::kIdentityNotFound);
}

TEST_F(KcerCertificateStoreTest, CommitIdentity_EmptyFinalName) {
  scoped_refptr<PrivateKey> key = CreateKey(kTestTempIdentityName);
  scoped_refptr<net::X509Certificate> cert = MakeCertForKey(*key);
  ASSERT_TRUE(cert);

  TestFuture<std::optional<StoreError>> future;
  store_->CommitIdentity(kTestTempIdentityName, /*final_identity_name=*/"",
                         cert, future.GetCallback());

  std::optional<StoreError> error = future.Take();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(error.value(), StoreError::kInvalidFinalIdentityName);
}

TEST_F(KcerCertificateStoreTest, CommitIdentity_NullCert) {
  CreateKey(kTestTempIdentityName);

  TestFuture<std::optional<StoreError>> future;
  store_->CommitIdentity(kTestTempIdentityName, kTestFinalIdentityName, nullptr,
                         future.GetCallback());

  std::optional<StoreError> error = future.Take();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(error.value(), StoreError::kInvalidCertificateInput);
}

// --- GetIdentity tests ---

TEST_F(KcerCertificateStoreTest, GetIdentity_NotFound) {
  TestFuture<StoreErrorOr<std::optional<ClientIdentity>>> future;
  store_->GetIdentity(kTestIdentityName, future.GetCallback());

  StoreErrorOr<std::optional<ClientIdentity>> result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result.value().has_value());
}

TEST_F(KcerCertificateStoreTest, GetIdentity_CorruptedSpki) {
  // Store invalid base64 as the SPKI.
  base::DictValue identity;
  identity.Set(kSpkiKey, "!!!not_valid_base64!!!");
  pref_service_.SetDict(kTestIdentityName, std::move(identity));

  TestFuture<StoreErrorOr<std::optional<ClientIdentity>>> future;
  store_->GetIdentity(kTestIdentityName, future.GetCallback());

  StoreErrorOr<std::optional<ClientIdentity>> result = future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), StoreError::kLoadKeyFailed);
}

TEST_F(KcerCertificateStoreTest, GetIdentity_KeyNoLongerExists) {
  // A syntactically valid SPKI that was never generated in Kcer: the real
  // DoesPrivateKeyExist check returns false, so the load fails.
  const std::vector<uint8_t> unknown_spki(91, 0xAB);
  base::DictValue identity;
  identity.Set(kSpkiKey, base::Base64Encode(unknown_spki));
  identity.Set(kKeySource, static_cast<int>(PrivateKeySource::kChromeOsHwKey));
  pref_service_.SetDict(kTestIdentityName, std::move(identity));

  TestFuture<StoreErrorOr<std::optional<ClientIdentity>>> future;
  store_->GetIdentity(kTestIdentityName, future.GetCallback());

  StoreErrorOr<std::optional<ClientIdentity>> result = future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), StoreError::kLoadKeyFailed);
}

TEST_F(KcerCertificateStoreTest, GetIdentity_ReturnsBoundCertAfterCommit) {
  // Full real round-trip: generate a key, commit a matching cert, then load the
  // identity back and confirm the cert bound during key load is surfaced via
  // PrivateKey::GetBoundCert().
  scoped_refptr<PrivateKey> key = CreateKey(kTestIdentityName);
  const std::vector<uint8_t> spki = key->GetSubjectPublicKeyInfo();
  scoped_refptr<net::X509Certificate> cert = MakeCertForKey(*key);
  ASSERT_TRUE(cert);

  TestFuture<std::optional<StoreError>> commit_future;
  store_->CommitCertificate(kTestIdentityName, cert,
                            commit_future.GetCallback());
  ASSERT_FALSE(commit_future.Take().has_value());

  TestFuture<StoreErrorOr<std::optional<ClientIdentity>>> future;
  store_->GetIdentity(kTestIdentityName, future.GetCallback());

  StoreErrorOr<std::optional<ClientIdentity>> result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value().has_value());
  const ClientIdentity& identity = result.value().value();
  EXPECT_EQ(identity.name, kTestIdentityName);
  ASSERT_TRUE(identity.private_key);
  EXPECT_EQ(identity.private_key->GetSubjectPublicKeyInfo(), spki);
  ASSERT_TRUE(identity.certificate);
  EXPECT_TRUE(identity.is_valid());
}

// --- DeleteIdentities tests ---

TEST_F(KcerCertificateStoreTest, DeleteIdentities_Success) {
  CreateKey(kTestIdentityName);
  ASSERT_FALSE(pref_service_.GetDict(kTestIdentityName).empty());

  TestFuture<std::optional<StoreError>> future;
  store_->DeleteIdentities({kTestIdentityName}, future.GetCallback());

  EXPECT_FALSE(future.Take().has_value());
  EXPECT_TRUE(pref_service_.GetDict(kTestIdentityName).empty());
}

TEST_F(KcerCertificateStoreTest, DeleteIdentities_EmptyName) {
  TestFuture<std::optional<StoreError>> future;
  store_->DeleteIdentities({""}, future.GetCallback());

  std::optional<StoreError> error = future.Take();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(error.value(), StoreError::kInvalidIdentityName);
}

// --- CreateForProfile gating tests ---

// CreateForProfile() consults ash::ProfileHelper to classify the profile's
// user and only provisions a store for a regular signed-in user. The ineligible
// sessions exercised here short-circuit (on the user-type / missing-user check)
// before reaching kcer::KcerFactoryAsh, which would require the Kcer/Chaps
// environment that only exists in browser tests. A FakeChromeUserManager plus a
// TestingProfileManager is therefore enough to drive these early-returns, and
// it lets the test map a profile to a real user_manager::User so the rejection
// is genuinely driven by the user's type rather than a missing mapping.
class KcerCertificateStoreCreateForProfileTest : public testing::Test {
 protected:
  KcerCertificateStoreCreateForProfileTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        user_manager_(new ash::FakeChromeUserManager()),
        scoped_user_manager_(base::WrapUnique(user_manager_.get())) {}

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;
};

// A guest session is not a regular signed-in user, so it must not receive a
// CertificateStore.
TEST_F(KcerCertificateStoreCreateForProfileTest, GuestProfileGetsNoStore) {
  user_manager_->AddGuestUser();
  user_manager_->LoginUser(user_manager::GuestAccountId());
  TestingProfile* guest_profile = profile_manager_.CreateGuestProfile();
  ASSERT_TRUE(guest_profile);

  // Confirm the profile genuinely maps to a guest user_manager::User, so the
  // rejection below is driven by the user-type check rather than the
  // missing-user early return (which UnmappedProfileGetsNoStore covers).
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(guest_profile);
  ASSERT_TRUE(user);
  ASSERT_EQ(user->GetType(), user_manager::UserType::kGuest);

  EXPECT_FALSE(KcerCertificateStore::CreateForProfile(guest_profile));
}

// A profile with no associated user_manager::User (e.g. not signed in) is
// likewise ineligible.
TEST_F(KcerCertificateStoreCreateForProfileTest, UnmappedProfileGetsNoStore) {
  TestingProfile* profile =
      profile_manager_.CreateTestingProfile("unmapped_profile");
  ASSERT_TRUE(profile);

  // No user is logged in, so the profile maps to no user.
  ASSERT_FALSE(ash::ProfileHelper::Get()->GetUserByProfile(profile));

  EXPECT_FALSE(KcerCertificateStore::CreateForProfile(profile));
}

// Deleting a non-permanent (e.g. temporary) identity removes only the named key
// by SPKI and does NOT trigger the browser enterprise client certificate key
// sweep, so unrelated keys on the same token are left intact.
TEST_F(KcerCertificateStoreTest, DeleteIdentities_LeavesUnrelatedKey) {
  scoped_refptr<PrivateKey> named = CreateKey(kTestIdentityName);
  const std::vector<uint8_t> unrelated_spki = GenerateUntaggedKeyInKcer();
  ASSERT_TRUE(KeyExistsInKcer(named->GetSubjectPublicKeyInfo()));
  ASSERT_TRUE(KeyExistsInKcer(unrelated_spki));

  TestFuture<std::optional<StoreError>> future;
  store_->DeleteIdentities({kTestIdentityName}, future.GetCallback());
  EXPECT_FALSE(future.Take().has_value());

  EXPECT_FALSE(KeyExistsInKcer(named->GetSubjectPublicKeyInfo()));
  EXPECT_TRUE(KeyExistsInKcer(unrelated_spki));
}

// Deleting a permanent managed identity removes the named key and runs the
// browser enterprise client certificate key sweep to completion. The softoken
// test slot can't write the browser enterprise tag, so the sweep finds nothing
// extra to reap here; the positive reap path is covered against a fake chaps
// client in kcer_token_impl_unittest.cc.
TEST_F(KcerCertificateStoreTest, DeleteIdentities_PermanentNameRemovesKey) {
  scoped_refptr<PrivateKey> key = CreateKey(kManagedProfileIdentityName);
  const std::vector<uint8_t> spki = key->GetSubjectPublicKeyInfo();
  ASSERT_TRUE(KeyExistsInKcer(spki));

  TestFuture<std::optional<StoreError>> future;
  store_->DeleteIdentities({kManagedProfileIdentityName}, future.GetCallback());
  EXPECT_FALSE(future.Take().has_value());

  EXPECT_FALSE(KeyExistsInKcer(spki));
  EXPECT_TRUE(pref_service_.GetDict(kManagedProfileIdentityName).empty());
}

}  // namespace

}  // namespace client_certificates
