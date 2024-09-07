// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/platform_keys/platform_keys_service.h"

#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service_test_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ash/scoped_test_system_nss_key_slot_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/chaps_util/test_util.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "crypto/nss_key_util.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/sha2.h"
#include "crypto/signature_verifier.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/constants/pkcs11_custom_attributes.h"

namespace ash::platform_keys {
namespace {

using ::chromeos::platform_keys::HashAlgorithm;
using ::chromeos::platform_keys::KeyAttributeType;
using ::chromeos::platform_keys::Status;
using ::chromeos::platform_keys::TokenId;

constexpr char kTestUserEmail[] = "test@example.com";
constexpr char kTestAffiliationId[] = "test_affiliation_id";
const std::vector<uint8_t> kTestingData = {9, 8, 7, 6, 5, 0, 1, 2, 3};
const std::vector<uint8_t> kSymId = {1, 2, 3, 0, 1, 2, 3};
const std::vector<uint8_t> kSymId2 = {1, 0, 3, 0, 7, 0, 9};
const int kDefaultSymKeySize = 32;
const unsigned long kDefaultSymSignatureSize = 32;

enum class ProfileToUse {
  // A Profile that belongs to a user that is not affiliated with the device (no
  // access to the system token).
  kUnaffiliatedUserProfile,
  // A Profile that belongs to a user that is affiliated with the device (access
  // to the system token).
  kAffiliatedUserProfile,
  // The sign-in screen profile.
  kSigninProfile
};

// Describes a test configuration for the test suite that runs one test per
// profile.
struct TestConfigPerProfile {
  // The profile for which PlatformKeysService should be tested.
  ProfileToUse profile_to_use;

  // The token IDs that are expected to be available for |profile_to_use|.
  std::vector<TokenId> token_ids;
};

// Describes a test configuration for the test suite that runs one test per
// (profile, token) combination.
struct TestConfigPerToken {
  // The profile for which PlatformKeysService should be tested.
  ProfileToUse profile_to_use;

  // The token ID to perform the tests on.
  TokenId token_id;
};

// Returns |hash| prefixed with DER-encoded PKCS#1 DigestInfo with
// AlgorithmIdentifier=id-sha256.
// This is useful for testing PlatformKeysService::SignRSAPKCS1Raw which only
// appends PKCS#1 v1.5 padding before signing.
std::vector<uint8_t> PrependSHA256DigestInfo(base::span<const uint8_t> hash) {
  // DER-encoded PKCS#1 DigestInfo "prefix" with
  // AlgorithmIdentifier=id-sha256.
  // The encoding is taken from https://tools.ietf.org/html/rfc3447#page-43
  const std::vector<uint8_t> kDigestInfoSha256DerData = {
      0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
      0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20};

  std::vector<uint8_t> result;
  result.reserve(kDigestInfoSha256DerData.size() + hash.size());

  result.insert(result.end(), kDigestInfoSha256DerData.begin(),
                kDigestInfoSha256DerData.end());
  result.insert(result.end(), hash.begin(), hash.end());
  return result;
}

std::string BytesToStr(const std::vector<uint8_t>& val) {
  return std::string(val.begin(), val.end());
}

}  // namespace

class PlatformKeysServiceBrowserTestBase
    : public MixinBasedInProcessBrowserTest {
 public:
  PlatformKeysServiceBrowserTestBase() = default;
  PlatformKeysServiceBrowserTestBase(
      const PlatformKeysServiceBrowserTestBase& other) = delete;
  PlatformKeysServiceBrowserTestBase& operator=(
      const PlatformKeysServiceBrowserTestBase& other) = delete;
  ~PlatformKeysServiceBrowserTestBase() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    // Call |Request*PolicyUpdate| even if not setting affiliation IDs so
    // (empty) policy blobs are prepared in FakeSessionManagerClient.
    auto user_policy_update = user_policy_mixin_.RequestPolicyUpdate();
    auto device_policy_update = device_state_mixin_.RequestDevicePolicyUpdate();
    if (GetProfileToUse() == ProfileToUse::kAffiliatedUserProfile) {
      device_policy_update->policy_data()->add_device_affiliation_ids(
          kTestAffiliationId);
      user_policy_update->policy_data()->add_user_affiliation_ids(
          kTestAffiliationId);
    }

    PlatformKeysServiceFactory::GetInstance()->SetTestingMode(true);
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();

    if (GetProfileToUse() == ProfileToUse::kSigninProfile) {
      profile_ = ProfileHelper::GetSigninProfile();
    } else {
      ASSERT_TRUE(login_manager_mixin_.LoginAndWaitForActiveSession(
          LoginManagerMixin::CreateDefaultUserContext(test_user_info_)));
      profile_ = ProfileManager::GetActiveUserProfile();

      base::RunLoop loop;
      NssServiceFactory::GetForContext(profile_)
          ->UnsafelyGetNSSCertDatabaseForTesting(
              base::BindOnce(&PlatformKeysServiceBrowserTestBase::SetUserSlot,
                             base::Unretained(this), loop.QuitClosure()));
      loop.Run();
    }
    ASSERT_TRUE(profile_);

    platform_keys_service_ =
        PlatformKeysServiceFactory::GetForBrowserContext(profile_);
    ASSERT_TRUE(platform_keys_service_);
  }

  void TearDownOnMainThread() override {
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  virtual ProfileToUse GetProfileToUse() = 0;

  PlatformKeysService* platform_keys_service() {
    return platform_keys_service_;
  }

  // Returns the slot to be used depending on |token_id|.
  PK11SlotInfo* GetSlot(TokenId token_id) {
    switch (token_id) {
      case TokenId::kSystem:
        return system_nss_key_slot_mixin_.slot();
      case TokenId::kUser:
        return user_slot_.get();
    }
  }

  // Generates a key pair in the given |token_id| using platform keys service
  // and returns the SubjectPublicKeyInfo string encoded in DER format.
  std::vector<uint8_t> GenerateKeyPair(TokenId token_id,
                                       unsigned int key_size) {
    base::test::TestFuture<std::vector<uint8_t>,
                           chromeos::platform_keys::Status>
        generate_key_waiter;
    platform_keys_service()->GenerateRSAKey(token_id, key_size,
                                            /*sw_backed=*/false,
                                            generate_key_waiter.GetCallback());
    EXPECT_TRUE(generate_key_waiter.Wait());
    return std::get<std::vector<uint8_t>>(generate_key_waiter.Take());
  }

  // Generates a key pair with a default size in the given |token_id| using
  // platform keys service and returns the SubjectPublicKeyInfo string encoded
  // in DER format.
  std::vector<uint8_t> GenerateKeyPair(TokenId token_id) {
    const unsigned int kDefaultKeySize = 2048;
    return GenerateKeyPair(token_id, kDefaultKeySize);
  }

  // Imports the certificate and key described by the |cert_filename| and
  // |key_filename| files in |source_dir| into the Token |token_id|, then stores
  // the resulting certificate in *|out_cert| and the SPKI of the public key in
  // *|out_spki_der|. Should be wrapped in ASSERT_NO_FATAL_FAILURE.
  void ImportCertAndKey(TokenId token_id,
                        const base::FilePath& source_dir,
                        const std::string& cert_filename,
                        const std::string& key_filename,
                        net::ScopedCERTCertificate* out_cert,
                        std::vector<uint8_t>* out_spki_der) {
    // Import testing key pair and certificate.
    {
      base::ScopedAllowBlockingForTesting allow_io;
      net::ImportClientCertAndKeyFromFile(
          source_dir, cert_filename, key_filename, GetSlot(token_id), out_cert);
    }
    CERTCertificate* cert = out_cert->get();
    ASSERT_TRUE(cert);
    ASSERT_GT(cert->derPublicKey.len, 0U);
    *out_spki_der =
        std::vector<uint8_t>(cert->derPublicKey.data,
                             cert->derPublicKey.data + cert->derPublicKey.len);
  }

 private:
  void SetUserSlot(base::OnceClosure done_callback, net::NSSCertDatabase* db) {
    user_slot_ = db->GetPrivateSlot();
    std::move(done_callback).Run();
  }

  const AccountId test_user_account_id_ = AccountId::FromUserEmailGaiaId(
      kTestUserEmail,
      signin::GetTestGaiaIdForEmail(kTestUserEmail));
  const LoginManagerMixin::TestUserInfo test_user_info_{test_user_account_id_};

  ScopedTestSystemNSSKeySlotMixin system_nss_key_slot_mixin_{&mixin_host_};
  LoginManagerMixin login_manager_mixin_{&mixin_host_, {test_user_info_}};

  DeviceStateMixin device_state_mixin_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_user_account_id_};

  // Unowned pointer to the profile selected by the current TestConfig.
  // Valid after SetUpOnMainThread().
  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;
  // Unowned pointer to the PlatformKeysService for |profile_|. Valid after
  // SetUpOnMainThread().
  raw_ptr<PlatformKeysService, DanglingUntriaged> platform_keys_service_ =
      nullptr;
  // The private slot for the profile under test. This should be null if the
  // test parameter mandates testing with the sign-in profile.
  crypto::ScopedPK11Slot user_slot_;
};

class PlatformKeysServicePerProfileBrowserTest
    : public PlatformKeysServiceBrowserTestBase,
      public ::testing::WithParamInterface<TestConfigPerProfile> {
 public:
  PlatformKeysServicePerProfileBrowserTest() = default;
  PlatformKeysServicePerProfileBrowserTest(
      const PlatformKeysServicePerProfileBrowserTest& other) = delete;
  PlatformKeysServicePerProfileBrowserTest& operator=(
      const PlatformKeysServicePerProfileBrowserTest& other) = delete;
  ~PlatformKeysServicePerProfileBrowserTest() override = default;

 protected:
  ProfileToUse GetProfileToUse() override { return GetParam().profile_to_use; }
};

using CallbackIfCallableBrowserTest = InProcessBrowserTest;

// Tests that GetTokens() is callable and returns the expected tokens.
IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerProfileBrowserTest, GetTokens) {
  test_util::GetTokensExecutionWaiter get_tokens_waiter;
  platform_keys_service()->GetTokens(get_tokens_waiter.GetCallback());
  ASSERT_TRUE(get_tokens_waiter.Wait());

  EXPECT_EQ(get_tokens_waiter.status(), Status::kSuccess);
  EXPECT_THAT(get_tokens_waiter.token_ids(),
              ::testing::UnorderedElementsAreArray(GetParam().token_ids));
}

// Generates a key pair in tokens accessible from the profile under test and
// retrieves them.
IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerProfileBrowserTest, GetAllKeys) {
  // Generate key pair in every token.
  std::map<TokenId, std::vector<uint8_t>> token_key_map;
  for (TokenId token_id : GetParam().token_ids) {
    const std::vector<uint8_t> public_key_spki_der = GenerateKeyPair(token_id);
    ASSERT_FALSE(public_key_spki_der.empty());
    token_key_map[token_id] = public_key_spki_der;
  }

  // Only keys in the requested token should be retrieved.
  for (TokenId token_id : GetParam().token_ids) {
    base::test::TestFuture<std::vector<std::vector<uint8_t>>, Status>
        get_all_keys_waiter;
    platform_keys_service()->GetAllKeys(token_id,
                                        get_all_keys_waiter.GetCallback());
    ASSERT_TRUE(get_all_keys_waiter.Wait());

    EXPECT_EQ(get_all_keys_waiter.Get<Status>(), Status::kSuccess);
    const auto& public_keys =
        get_all_keys_waiter.Get<std::vector<std::vector<uint8_t>>>();
    ASSERT_EQ(public_keys.size(), 1U);
    EXPECT_EQ(public_keys[0], token_key_map[token_id]);
  }
}

// Imports the same key into all tokens. Verifies that key attributes are stored
// per-token and don't leak between tokens.
IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerProfileBrowserTest,
                       KeyAttributesPerToken) {
  // Import the same key pair + cert in every token, remember its SPKI.
  std::vector<uint8_t> spki_der;
  for (TokenId token_id : GetParam().token_ids) {
    net::ScopedCERTCertificate cert;
    std::vector<uint8_t> current_spki_der;
    ASSERT_NO_FATAL_FAILURE(
        ImportCertAndKey(token_id, net::GetTestCertsDirectory(), "client_1.pem",
                         "client_1.pk8", &cert, &current_spki_der));
    // The SPKI must be the same on every slot, because the same key was
    // imported.
    if (!spki_der.empty()) {
      EXPECT_EQ(current_spki_der, spki_der);
      continue;
    }
    spki_der = current_spki_der;
  }

  // Set an attribute for the key on each token.
  const KeyAttributeType kAttributeType =
      KeyAttributeType::kCertificateProvisioningId;
  std::map<TokenId, std::vector<uint8_t>> token_to_value;
  for (TokenId token_id : GetParam().token_ids) {
    token_to_value[token_id] = {1, 2, 3, 4, 5, static_cast<uint8_t>(token_id)};

    // Set key attribute.
    base::test::TestFuture<Status> set_attr_waiter;
    platform_keys_service()->SetAttributeForKey(
        token_id, spki_der, kAttributeType, token_to_value[token_id],
        set_attr_waiter.GetCallback());
    ASSERT_TRUE(set_attr_waiter.Wait());
    EXPECT_EQ(set_attr_waiter.Get<Status>(), Status::kSuccess);
  }

  // Verify the token-specific attribute value for the key on each token.
  for (TokenId token_id : GetParam().token_ids) {
    // Get key attribute.
    base::test::TestFuture<std::optional<std::vector<uint8_t>>, Status>
        get_attr_waiter;
    platform_keys_service()->GetAttributeForKey(
        token_id, spki_der, kAttributeType, get_attr_waiter.GetCallback());
    ASSERT_TRUE(get_attr_waiter.Wait());

    EXPECT_EQ(get_attr_waiter.Get<Status>(), Status::kSuccess);
    const std::optional<std::vector<uint8_t>>& attr = get_attr_waiter.Get<0>();
    ASSERT_TRUE(attr.has_value());
    EXPECT_EQ(attr.value(), token_to_value[token_id]);
  }
}

IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerProfileBrowserTest,
                       IsKeyOnTokenWhenNot) {
  // More than one available token are needed to test this.
  if (GetParam().token_ids.size() < 2) {
    return;
  }

  const TokenId token_id_1 = GetParam().token_ids[0];
  const TokenId token_id_2 = GetParam().token_ids[1];

  const std::vector<uint8_t> public_key = GenerateKeyPair(token_id_1);

  test_util::IsKeyOnTokenExecutionWaiter is_key_on_token_waiter;
  platform_keys_service()->IsKeyOnToken(token_id_2, std::move(public_key),
                                        is_key_on_token_waiter.GetCallback());
  ASSERT_TRUE(is_key_on_token_waiter.Wait());

  EXPECT_EQ(is_key_on_token_waiter.status(), Status::kSuccess);
  ASSERT_TRUE(is_key_on_token_waiter.on_slot().has_value());
  EXPECT_FALSE(is_key_on_token_waiter.on_slot().value());
}

INSTANTIATE_TEST_SUITE_P(
    AllSupportedProfileTypes,
    PlatformKeysServicePerProfileBrowserTest,
    ::testing::Values(
        TestConfigPerProfile{ProfileToUse::kSigninProfile, {TokenId::kSystem}},
        TestConfigPerProfile{ProfileToUse::kUnaffiliatedUserProfile,
                             {TokenId::kUser}},
        TestConfigPerProfile{ProfileToUse::kAffiliatedUserProfile,
                             {TokenId::kUser, TokenId::kSystem}},
        TestConfigPerProfile{ProfileToUse::kAffiliatedUserProfile,
                             {TokenId::kSystem, TokenId::kUser}}));

class PlatformKeysServicePerTokenBrowserTest
    : public PlatformKeysServiceBrowserTestBase,
      public ::testing::WithParamInterface<TestConfigPerToken> {
 public:
  PlatformKeysServicePerTokenBrowserTest() = default;
  PlatformKeysServicePerTokenBrowserTest(
      const PlatformKeysServicePerTokenBrowserTest& other) = delete;
  PlatformKeysServicePerTokenBrowserTest& operator=(
      const PlatformKeysServicePerTokenBrowserTest& other) = delete;
  ~PlatformKeysServicePerTokenBrowserTest() override = default;

 protected:
  ProfileToUse GetProfileToUse() override { return GetParam().profile_to_use; }
};

// Generates a Rsa key pair and tests signing using that key pair.
IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       GenerateRsaAndSign) {
  const std::vector<uint8_t> kDataToSign({1, 2, 3, 4, 5});
  const unsigned int kKeySize = 2048;
  const HashAlgorithm kHashAlgorithm = HashAlgorithm::HASH_ALGORITHM_SHA256;
  const crypto::SignatureVerifier::SignatureAlgorithm kSignatureAlgorithm =
      crypto::SignatureVerifier::RSA_PKCS1_SHA256;

  const TokenId token_id = GetParam().token_id;
  base::test::TestFuture<std::vector<uint8_t>, Status> generate_key_waiter;
  platform_keys_service()->GenerateRSAKey(token_id, kKeySize,
                                          /*sw_backed=*/false,
                                          generate_key_waiter.GetCallback());
  ASSERT_TRUE(generate_key_waiter.Wait());
  EXPECT_EQ(generate_key_waiter.Get<Status>(), Status::kSuccess);

  const std::vector<uint8_t> public_key_spki_der =
      generate_key_waiter.Get<std::vector<uint8_t>>();
  EXPECT_FALSE(public_key_spki_der.empty());

  base::test::TestFuture<std::vector<uint8_t>, Status> sign_waiter;
  platform_keys_service()->SignRsaPkcs1(token_id, kDataToSign,
                                        public_key_spki_der, kHashAlgorithm,
                                        sign_waiter.GetCallback());
  ASSERT_TRUE(sign_waiter.Wait());
  EXPECT_EQ(sign_waiter.Get<Status>(), Status::kSuccess);

  crypto::SignatureVerifier signature_verifier;
  ASSERT_TRUE(signature_verifier.VerifyInit(
      kSignatureAlgorithm,
      base::as_bytes(base::make_span(sign_waiter.Get<std::vector<uint8_t>>())),
      base::as_bytes(base::make_span(public_key_spki_der))));
  signature_verifier.VerifyUpdate(base::as_bytes(base::make_span(kDataToSign)));
  EXPECT_TRUE(signature_verifier.VerifyFinal());
}

class RunLoopQuiter {
 public:
  explicit RunLoopQuiter(base::RunLoop* rl) : runloop_(rl) {}
  void Quit() { runloop_->Quit(); }
  void QuitWithFail() {
    Quit();
    FAIL();
  }
  base::WeakPtr<RunLoopQuiter> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  const raw_ptr<base::RunLoop> runloop_;
  base::WeakPtrFactory<RunLoopQuiter> weak_factory_{this};
};

// Verifies that RunCallBackIfCallableElseCleanup will run the cleanup callback
// if the given callback is canceled. RunCallBackIfCallableElseCleanup is
// specially helpful in PlatformKeyService when PKS is asked to generate a key
// and by the time the key is generated the asker is gone.
IN_PROC_BROWSER_TEST_F(CallbackIfCallableBrowserTest,
                       CallCleanUpWhenCallBackIsCanceled) {
  base::RunLoop loop;
  base::OnceCallback<void()> canceled_cb, quit_loop_cb;

  RunLoopQuiter main_quiter(&loop);
  quit_loop_cb = base::BindOnce(&RunLoopQuiter::Quit, main_quiter.GetWeakPtr());

  {
    RunLoopQuiter canceled_quiter(&loop);
    // If the canceled_cb runs then test will fail.
    canceled_cb = base::BindOnce(&RunLoopQuiter::QuitWithFail,
                                 canceled_quiter.GetWeakPtr());
  }

  RunCallBackIfCallableElseRunCleanUp(std::move(canceled_cb),
                                      std::move(quit_loop_cb));
  loop.Run();
}

// Generates a Rsa key pair and tests signing using the SignRSAPKCS1Raw
// function.
IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       GenerateRsaAndSignRaw) {
  const unsigned int kKeySize = 2048;
  const TokenId token_id = GetParam().token_id;

  // SignRSAPKCS1Raw only performs PKCS#1.5 padding. To get a correct PKCS#1
  // signature of |kDataToSign|, it is necessary to pass
  // (DigestInfo + hash(kDataToSign)) to SignRSAPKCS1Raw, where DigestInfo
  // describes the hash function.
  const std::vector<uint8_t> kDataToSign({1, 2, 3, 4, 5});
  const std::array<uint8_t, crypto::kSHA256Length> kDataToSignHash =
      crypto::SHA256Hash(kDataToSign);
  const std::vector<uint8_t> kDigestInfoAndDataToSignHash =
      PrependSHA256DigestInfo(kDataToSignHash);

  const crypto::SignatureVerifier::SignatureAlgorithm kSignatureAlgorithm =
      crypto::SignatureVerifier::RSA_PKCS1_SHA256;

  const std::vector<uint8_t> public_key_spki_der =
      GenerateKeyPair(token_id, kKeySize);

  base::test::TestFuture<std::vector<uint8_t>, Status> sign_waiter;
  platform_keys_service()->SignRSAPKCS1Raw(
      token_id, kDigestInfoAndDataToSignHash, public_key_spki_der,
      sign_waiter.GetCallback());
  ASSERT_TRUE(sign_waiter.Wait());
  EXPECT_EQ(sign_waiter.Get<Status>(), Status::kSuccess);

  crypto::SignatureVerifier signature_verifier;
  ASSERT_TRUE(signature_verifier.VerifyInit(
      kSignatureAlgorithm,
      base::as_bytes(base::make_span(sign_waiter.Get<std::vector<uint8_t>>())),
      base::as_bytes(base::make_span(public_key_spki_der))));
  signature_verifier.VerifyUpdate(kDataToSign);
  EXPECT_TRUE(signature_verifier.VerifyFinal());
}

// Generates a software-backed RSA key pair.
IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       GenerateRsaSoftwareBacked) {
  const unsigned int kKeySize = 2048;

  // Arrange: Configure the ChapsUtilFactory singleton instance to return fake
  // ChapsUtil instances.
  chromeos::ScopedChapsUtilOverride scoped_chaps_util_override;

  // Act: Generate the key pair.
  const TokenId token_id = GetParam().token_id;
  base::test::TestFuture<std::vector<uint8_t>, Status> generate_key_waiter;
  platform_keys_service()->GenerateRSAKey(token_id, kKeySize,
                                          /*sw_backed=*/true,
                                          generate_key_waiter.GetCallback());
  ASSERT_TRUE(generate_key_waiter.Wait());
  EXPECT_EQ(generate_key_waiter.Get<Status>(), Status::kSuccess);

  // Assert: Verify that the the returned public key SPKI has been generated
  // through the fake ChapsUtil.
  const std::vector<uint8_t> public_key_spki_der =
      generate_key_waiter.Get<std::vector<uint8_t>>();
  EXPECT_FALSE(public_key_spki_der.empty());

  EXPECT_THAT(scoped_chaps_util_override.generated_key_spkis(),
              ::testing::ElementsAre(BytesToStr(public_key_spki_der)));
}

// Generates a Rsa key pair and tests expected limits of the input length of the
// SignRSAPKCS1Raw function.
IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       SignRawInputTooLong) {
  const unsigned int kKeySize = 2048;
  const TokenId token_id = GetParam().token_id;

  const std::vector<uint8_t> public_key_spki_der =
      GenerateKeyPair(token_id, kKeySize);

  // SignRSAPKCS1Raw performs PKCS#11 padding which adds at least 11 bytes.
  {
    // An input of |kKeySize in bytes - 11| should be fine.
    std::vector<uint8_t> data_to_sign;
    data_to_sign.resize(kKeySize / 8 - 11);

    base::test::TestFuture<std::vector<uint8_t>, Status> sign_waiter;
    platform_keys_service()->SignRSAPKCS1Raw(
        token_id, data_to_sign, public_key_spki_der, sign_waiter.GetCallback());
    ASSERT_TRUE(sign_waiter.Wait());
    EXPECT_EQ(sign_waiter.Get<Status>(), Status::kSuccess);
  }

  {
    // An input of |kKeySize in bytes - 10| should be too long.
    std::vector<uint8_t> data_to_sign_too_long;
    data_to_sign_too_long.resize(kKeySize / 8 - 10);

    base::test::TestFuture<std::vector<uint8_t>, Status> sign_waiter;
    platform_keys_service()->SignRSAPKCS1Raw(token_id, data_to_sign_too_long,
                                             public_key_spki_der,
                                             sign_waiter.GetCallback());
    ASSERT_TRUE(sign_waiter.Wait());
    EXPECT_EQ(sign_waiter.Get<Status>(), Status::kErrorInputTooLong);
  }
}

IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       SetAndGetKeyAttribute) {
  // The attribute type to be set and retrieved using platform keys service.
  const KeyAttributeType kAttributeType =
      KeyAttributeType::kCertificateProvisioningId;
  const TokenId token_id = GetParam().token_id;
  const std::vector<uint8_t> kAttributeValue = {20, 21, 22, 23, 24};

  // Generate key pair.
  const std::vector<uint8_t> public_key_spki_der = GenerateKeyPair(token_id);
  ASSERT_FALSE(public_key_spki_der.empty());

  // Set key attribute.
  base::test::TestFuture<Status> set_attr_waiter;
  platform_keys_service()->SetAttributeForKey(token_id, public_key_spki_der,
                                              kAttributeType, kAttributeValue,
                                              set_attr_waiter.GetCallback());
  ASSERT_EQ(set_attr_waiter.Get<Status>(), Status::kSuccess);

  // Get key attribute.
  base::test::TestFuture<std::optional<std::vector<uint8_t>>, Status>
      get_attr_waiter;
  platform_keys_service()->GetAttributeForKey(token_id, public_key_spki_der,
                                              kAttributeType,
                                              get_attr_waiter.GetCallback());
  ASSERT_TRUE(get_attr_waiter.Wait());

  EXPECT_EQ(get_attr_waiter.Get<Status>(), Status::kSuccess);
  std::optional<std::vector<uint8_t>> attr = get_attr_waiter.Get<0>();
  ASSERT_TRUE(attr.has_value());
  EXPECT_EQ(attr.value(), kAttributeValue);
}

// TODO(crbug.com/40686352): Add a test for an unset key attribute when
// simulating chaps behavior is possible.

IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       GetKeyAttributeForNonExistingKey) {
  const KeyAttributeType kAttributeType =
      KeyAttributeType::kCertificateProvisioningId;
  const TokenId token_id = GetParam().token_id;
  const std::vector<uint8_t> kNonExistingPublicKey = {1, 2, 3};

  // Get key attribute.
  base::test::TestFuture<std::optional<std::vector<uint8_t>>, Status>
      get_attr_waiter;
  platform_keys_service()->GetAttributeForKey(token_id, kNonExistingPublicKey,
                                              kAttributeType,
                                              get_attr_waiter.GetCallback());
  ASSERT_TRUE(get_attr_waiter.Wait());

  EXPECT_NE(get_attr_waiter.Get<Status>(), Status::kSuccess);
  EXPECT_FALSE(get_attr_waiter.Get<std::optional<std::vector<uint8_t>>>());
}

IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       SetKeyAttributeForNonExistingKey) {
  const KeyAttributeType kAttributeType =
      KeyAttributeType::kCertificateProvisioningId;
  const TokenId token_id = GetParam().token_id;
  const std::vector<uint8_t> kAttributeValue = {20, 21, 22, 23, 24};
  const std::vector<uint8_t> kNonExistingPublicKey = {1, 2, 3};

  // Set key attribute.
  base::test::TestFuture<Status> set_attr_waiter;
  platform_keys_service()->SetAttributeForKey(token_id, kNonExistingPublicKey,
                                              kAttributeType, kAttributeValue,
                                              set_attr_waiter.GetCallback());
  ASSERT_TRUE(set_attr_waiter.Wait());

  EXPECT_NE(set_attr_waiter.Get<Status>(), Status::kSuccess);
}

IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       RemoveKeyWithNoMatchingCertificates) {
  const TokenId token_id = GetParam().token_id;

  // Generate first key pair.
  const std::vector<uint8_t> public_key_1 = GenerateKeyPair(token_id);
  ASSERT_FALSE(public_key_1.empty());

  // Generate second key pair.
  const std::vector<uint8_t> public_key_2 = GenerateKeyPair(token_id);
  ASSERT_FALSE(public_key_2.empty());

  auto public_key_bytes_1 = base::as_bytes(base::make_span(public_key_1));
  auto public_key_bytes_2 = base::as_bytes(base::make_span(public_key_2));
  EXPECT_TRUE(crypto::FindNSSKeyFromPublicKeyInfo(public_key_bytes_1));
  EXPECT_TRUE(crypto::FindNSSKeyFromPublicKeyInfo(public_key_bytes_2));

  test_util::RemoveKeyExecutionWaiter remove_key_waiter;
  platform_keys_service()->RemoveKey(token_id, public_key_1,
                                     remove_key_waiter.GetCallback());
  ASSERT_TRUE(remove_key_waiter.Wait());

  EXPECT_EQ(remove_key_waiter.status(), Status::kSuccess);
  EXPECT_FALSE(crypto::FindNSSKeyFromPublicKeyInfo(public_key_bytes_1));
  EXPECT_TRUE(crypto::FindNSSKeyFromPublicKeyInfo(public_key_bytes_2));
}

IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       RemoveKeyWithMatchingCertificate) {
  const TokenId token_id = GetParam().token_id;

  // Assert that there are no certificates before importing.
  test_util::GetCertificatesExecutionWaiter get_certificates_waiter;
  platform_keys_service()->GetCertificates(
      token_id, get_certificates_waiter.GetCallback());
  ASSERT_TRUE(get_certificates_waiter.Wait());
  ASSERT_EQ(get_certificates_waiter.matches().size(), 0U);

  net::ScopedCERTCertificate cert;
  std::vector<uint8_t> public_key;
  ASSERT_NO_FATAL_FAILURE(
      ImportCertAndKey(token_id, net::GetTestCertsDirectory(), "client_1.pem",
                       "client_1.pk8", &cert, &public_key));

  // Assert that the certificate is imported correctly.
  ASSERT_TRUE(cert.get());
  test_util::GetCertificatesExecutionWaiter get_certificates_waiter_2;
  platform_keys_service()->GetCertificates(
      token_id, get_certificates_waiter_2.GetCallback());
  ASSERT_TRUE(get_certificates_waiter_2.Wait());
  ASSERT_EQ(get_certificates_waiter_2.matches().size(), 1U);

  auto public_key_bytes = base::as_bytes(base::make_span(public_key));
  EXPECT_TRUE(crypto::FindNSSKeyFromPublicKeyInfo(public_key_bytes));

  // Try Removing the key pair.
  test_util::RemoveKeyExecutionWaiter remove_key_waiter;
  platform_keys_service()->RemoveKey(token_id, public_key,
                                     remove_key_waiter.GetCallback());
  ASSERT_TRUE(remove_key_waiter.Wait());
  EXPECT_NE(remove_key_waiter.status(), Status::kSuccess);

  // Assert that the certificate is not removed.
  test_util::GetCertificatesExecutionWaiter get_certificates_waiter_3;
  platform_keys_service()->GetCertificates(
      token_id, get_certificates_waiter_3.GetCallback());
  ASSERT_TRUE(get_certificates_waiter_3.Wait());

  net::CertificateList found_certs = get_certificates_waiter_3.matches();
  ASSERT_EQ(found_certs.size(), 1U);
  EXPECT_TRUE(
      net::x509_util::IsSameCertificate(found_certs[0].get(), cert.get()));

  // Assert that the key pair is not deleted.
  EXPECT_TRUE(crypto::FindNSSKeyFromPublicKeyInfo(public_key_bytes));
}

IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       GetAllKeysWhenNoKeysGenerated) {
  const TokenId token_id = GetParam().token_id;
  base::test::TestFuture<std::vector<std::vector<uint8_t>>, Status>
      get_all_keys_waiter;
  platform_keys_service()->GetAllKeys(token_id,
                                      get_all_keys_waiter.GetCallback());
  ASSERT_TRUE(get_all_keys_waiter.Wait());

  EXPECT_EQ(get_all_keys_waiter.Get<Status>(), Status::kSuccess);
  const auto& public_keys =
      get_all_keys_waiter.Get<std::vector<std::vector<uint8_t>>>();
  EXPECT_TRUE(public_keys.empty());
}

IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest, IsKeyOnToken) {
  const TokenId token_id = GetParam().token_id;
  const std::vector<uint8_t> public_key = GenerateKeyPair(token_id);

  test_util::IsKeyOnTokenExecutionWaiter is_key_on_token_waiter;
  platform_keys_service()->IsKeyOnToken(token_id, std::move(public_key),
                                        is_key_on_token_waiter.GetCallback());
  ASSERT_TRUE(is_key_on_token_waiter.Wait());

  EXPECT_EQ(is_key_on_token_waiter.status(), Status::kSuccess);
  ASSERT_TRUE(is_key_on_token_waiter.on_slot().has_value());
  EXPECT_TRUE(is_key_on_token_waiter.on_slot().value());
}

IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       IsKeyOnTokenWhenNoKeysGenerated) {
  const TokenId token_id = GetParam().token_id;

  // A public key for a key that was never generated. (The content is also not
  // realistic.)
  std::vector<uint8_t> bad_key = {1, 2, 3, 4, 5};

  test_util::IsKeyOnTokenExecutionWaiter is_key_on_token_waiter;
  platform_keys_service()->IsKeyOnToken(token_id, std::move(bad_key),
                                        is_key_on_token_waiter.GetCallback());
  ASSERT_TRUE(is_key_on_token_waiter.Wait());

  EXPECT_EQ(is_key_on_token_waiter.status(), Status::kSuccess);
  ASSERT_TRUE(is_key_on_token_waiter.on_slot().has_value());
  EXPECT_FALSE(is_key_on_token_waiter.on_slot().value());
}

IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       GetKeyLocations) {
  const TokenId token_id = GetParam().token_id;

  const std::vector<uint8_t> public_key = GenerateKeyPair(token_id);

  test_util::GetKeyLocationsExecutionWaiter get_key_locations_waiter;
  platform_keys_service()->GetKeyLocations(
      public_key, get_key_locations_waiter.GetCallback());
  ASSERT_TRUE(get_key_locations_waiter.Wait());

  EXPECT_EQ(get_key_locations_waiter.status(), Status::kSuccess);
  ASSERT_EQ(get_key_locations_waiter.key_locations().size(), 1U);
  EXPECT_EQ(get_key_locations_waiter.key_locations()[0], token_id);
}

// Generates symmetric key and uses it to sign with HMAC.
IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest, SymKeySign) {
  const TokenId token_id = GetParam().token_id;
  base::test::TestFuture<std::vector<uint8_t>, Status> generate_key_waiter;
  platform_keys_service()->GenerateSymKey(
      token_id, kSymId, kDefaultSymKeySize,
      chromeos::platform_keys::SymKeyType::kHmac,
      generate_key_waiter.GetCallback());
  EXPECT_EQ(generate_key_waiter.Get<Status>(), Status::kSuccess);
  EXPECT_EQ(kSymId, generate_key_waiter.Get<std::vector<uint8_t>>());

  base::test::TestFuture<std::vector<uint8_t>, Status> sign_waiter;
  platform_keys_service()->SignWithSymKey(token_id, kTestingData, kSymId,
                                          sign_waiter.GetCallback());
  EXPECT_EQ(sign_waiter.Get<Status>(), Status::kSuccess);
  EXPECT_EQ(sign_waiter.Get<std::vector<uint8_t>>().size(),
            kDefaultSymSignatureSize);
}

// Cannot sign with invalid/absent key.
IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       SymKeySignInvalidKey) {
  const TokenId token_id = GetParam().token_id;

  // Should fail because the key doesn't exist.
  base::test::TestFuture<std::vector<uint8_t>, Status> sign_waiter;
  platform_keys_service()->SignWithSymKey(token_id, kTestingData, kSymId,
                                          sign_waiter.GetCallback());
  EXPECT_EQ(sign_waiter.Get<Status>(), Status::kErrorKeyNotFound);
  EXPECT_TRUE(sign_waiter.Get<std::vector<uint8_t>>().empty());

  base::test::TestFuture<std::vector<uint8_t>, Status> generate_key_waiter;
  platform_keys_service()->GenerateSymKey(
      token_id, kSymId, kDefaultSymKeySize,
      chromeos::platform_keys::SymKeyType::kAesCbc,
      generate_key_waiter.GetCallback());
  EXPECT_EQ(generate_key_waiter.Get<Status>(), Status::kSuccess);
  EXPECT_EQ(kSymId, generate_key_waiter.Get<std::vector<uint8_t>>());

  // Should fail because the key is generated for purposes other than signing.
  base::test::TestFuture<std::vector<uint8_t>, Status> sign_waiter2;
  platform_keys_service()->SignWithSymKey(token_id, kTestingData, kSymId,
                                          sign_waiter2.GetCallback());
  EXPECT_EQ(sign_waiter2.Get<Status>(), Status::kErrorInternal);
  EXPECT_TRUE(sign_waiter2.Get<std::vector<uint8_t>>().empty());
}

// Generates symmetric key and uses it to encrypt/decrypt with AES-CBC.
IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       SymKeyEncryptDecrypt) {
  const TokenId token_id = GetParam().token_id;
  base::test::TestFuture<std::vector<uint8_t>, Status> generate_key_waiter;
  platform_keys_service()->GenerateSymKey(
      token_id, kSymId, kDefaultSymKeySize,
      chromeos::platform_keys::SymKeyType::kAesCbc,
      generate_key_waiter.GetCallback());
  EXPECT_EQ(generate_key_waiter.Get<Status>(), Status::kSuccess);
  EXPECT_EQ(kSymId, generate_key_waiter.Get<std::vector<uint8_t>>());

  // Initialization vector must have a length of 16.
  const std::vector<uint8_t> kInitVecIncorrect(/*count=*/7, /*value=*/0);
  base::test::TestFuture<std::vector<uint8_t>, Status> encrypt_waiter;
  platform_keys_service()->EncryptAES(token_id, kTestingData, kSymId, "AES-CBC",
                                      kInitVecIncorrect,
                                      encrypt_waiter.GetCallback());
  EXPECT_EQ(encrypt_waiter.Get<Status>(), Status::kErrorAlgorithmNotSupported);

  const std::vector<uint8_t> kInitVec(/*count=*/16, /*value=*/0);
  base::test::TestFuture<std::vector<uint8_t>, Status> encrypt_waiter2;
  platform_keys_service()->EncryptAES(token_id, kTestingData, kSymId, "AES-CBC",
                                      kInitVec, encrypt_waiter2.GetCallback());
  EXPECT_EQ(encrypt_waiter2.Get<Status>(), Status::kSuccess);

  std::vector<uint8_t> encrypted_data =
      encrypt_waiter2.Get<std::vector<uint8_t>>();
  // Encrypted data's length should be divisible by 16.
  EXPECT_TRUE(encrypted_data.size() % 16 == 0);

  // Decrypting the resulting encrypted data to see if it matches the original
  // data.
  base::test::TestFuture<std::vector<uint8_t>, Status> decrypt_waiter;
  platform_keys_service()->DecryptAES(token_id, encrypted_data, kSymId,
                                      "AES-CBC", kInitVec,
                                      decrypt_waiter.GetCallback());
  EXPECT_EQ(decrypt_waiter.Get<Status>(), Status::kSuccess);
  EXPECT_EQ(kTestingData, decrypt_waiter.Get<std::vector<uint8_t>>());
}

// Cannot encrypt with invalid/absent key.
IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       SymKeyEncryptInvalidKey) {
  const TokenId token_id = GetParam().token_id;
  // Initialization vector must have a length of 16.
  const std::vector<uint8_t> kInitVec(/*count=*/16, /*value=*/0);

  // Should fail because the key doesn't exist.
  base::test::TestFuture<std::vector<uint8_t>, Status> encrypt_waiter;
  platform_keys_service()->EncryptAES(token_id, kTestingData, kSymId, "AES-CBC",
                                      kInitVec, encrypt_waiter.GetCallback());
  EXPECT_EQ(encrypt_waiter.Get<Status>(), Status::kErrorKeyNotFound);

  base::test::TestFuture<std::vector<uint8_t>, Status> generate_key_waiter;
  platform_keys_service()->GenerateSymKey(
      token_id, kSymId, kDefaultSymKeySize,
      chromeos::platform_keys::SymKeyType::kHmac,
      generate_key_waiter.GetCallback());
  EXPECT_EQ(generate_key_waiter.Get<Status>(), Status::kSuccess);
  EXPECT_EQ(kSymId, generate_key_waiter.Get<std::vector<uint8_t>>());

  // Should fail because the key is generated for purposes other than
  // encrypting/decrypting.
  base::test::TestFuture<std::vector<uint8_t>, Status> encrypt_waiter2;
  platform_keys_service()->EncryptAES(token_id, kTestingData, kSymId, "AES-CBC",
                                      kInitVec, encrypt_waiter2.GetCallback());
  EXPECT_EQ(encrypt_waiter2.Get<Status>(), Status::kErrorInternal);
}

// Cannot decrypt with invalid/absent key.
IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       SymKeyDecryptInvalidKey) {
  const TokenId token_id = GetParam().token_id;
  // Initialization vector must have a length of 16.
  const std::vector<uint8_t> kInitVec(/*count=*/16, /*value=*/0);

  // Should fail because the key doesn't exist.
  base::test::TestFuture<std::vector<uint8_t>, Status> decrypt_waiter;
  platform_keys_service()->DecryptAES(token_id, kTestingData, kSymId, "AES-CBC",
                                      kInitVec, decrypt_waiter.GetCallback());
  EXPECT_EQ(decrypt_waiter.Get<Status>(), Status::kErrorKeyNotFound);

  base::test::TestFuture<std::vector<uint8_t>, Status> generate_key_waiter;
  platform_keys_service()->GenerateSymKey(
      token_id, kSymId, kDefaultSymKeySize,
      chromeos::platform_keys::SymKeyType::kHmac,
      generate_key_waiter.GetCallback());
  EXPECT_EQ(generate_key_waiter.Get<Status>(), Status::kSuccess);
  EXPECT_EQ(kSymId, generate_key_waiter.Get<std::vector<uint8_t>>());

  // Should fail because the key is generated for purposes other than
  // encrypting/decrypting.
  base::test::TestFuture<std::vector<uint8_t>, Status> decrypt_waiter2;
  platform_keys_service()->DecryptAES(token_id, kTestingData, kSymId, "AES-CBC",
                                      kInitVec, decrypt_waiter2.GetCallback());
  EXPECT_EQ(decrypt_waiter2.Get<Status>(), Status::kErrorInternal);
}

// Generates symmetric key and then removes it.
IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest, SymRemoveKey) {
  const TokenId token_id = GetParam().token_id;

  base::test::TestFuture<std::vector<uint8_t>, Status> generate_key_waiter;
  platform_keys_service()->GenerateSymKey(
      token_id, kSymId, kDefaultSymKeySize,
      chromeos::platform_keys::SymKeyType::kAesCbc,
      generate_key_waiter.GetCallback());
  EXPECT_EQ(generate_key_waiter.Get<Status>(), Status::kSuccess);
  EXPECT_EQ(kSymId, generate_key_waiter.Get<std::vector<uint8_t>>());

  // Generating another key with the same key ID should fail.
  base::test::TestFuture<std::vector<uint8_t>, Status> generate_key_waiter2;
  platform_keys_service()->GenerateSymKey(
      token_id, kSymId, kDefaultSymKeySize,
      chromeos::platform_keys::SymKeyType::kHmac,
      generate_key_waiter2.GetCallback());
  EXPECT_EQ(generate_key_waiter2.Get<Status>(), Status::kErrorInternal);
  EXPECT_TRUE(generate_key_waiter2.Get<std::vector<uint8_t>>().empty());

  base::test::TestFuture<Status> remove_waiter;
  platform_keys_service()->RemoveSymKey(token_id, kSymId,
                                        remove_waiter.GetCallback());
  EXPECT_EQ(remove_waiter.Get<Status>(), Status::kSuccess);

  // Key is already removed, so this should fail.
  base::test::TestFuture<Status> remove_waiter2;
  platform_keys_service()->RemoveSymKey(token_id, kSymId,
                                        remove_waiter2.GetCallback());
  EXPECT_EQ(remove_waiter2.Get<Status>(), Status::kErrorKeyNotFound);
}

// Derives a symmetric key and then uses it to sign.
IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       SymKeyDeriveAndSign) {
  const TokenId token_id = GetParam().token_id;

  base::test::TestFuture<std::vector<uint8_t>, Status> generate_key_waiter;
  platform_keys_service()->GenerateSymKey(
      token_id, kSymId, kDefaultSymKeySize,
      chromeos::platform_keys::SymKeyType::kSp800Kdf,
      generate_key_waiter.GetCallback());
  EXPECT_EQ(generate_key_waiter.Get<Status>(), Status::kSuccess);
  EXPECT_EQ(kSymId, generate_key_waiter.Get<std::vector<uint8_t>>());

  base::test::TestFuture<std::vector<uint8_t>, Status> derive_waiter;
  platform_keys_service()->DeriveSymKey(
      token_id, kSymId, kSymId2, kTestingData, kTestingData,
      chromeos::platform_keys::SymKeyType::kHmac, derive_waiter.GetCallback());
  EXPECT_EQ(derive_waiter.Get<Status>(), Status::kSuccess);
  EXPECT_EQ(kSymId2, derive_waiter.Get<std::vector<uint8_t>>());

  base::test::TestFuture<std::vector<uint8_t>, Status> sign_waiter;
  platform_keys_service()->SignWithSymKey(token_id, kTestingData, kSymId2,
                                          sign_waiter.GetCallback());
  EXPECT_EQ(sign_waiter.Get<Status>(), Status::kSuccess);
  EXPECT_EQ(sign_waiter.Get<std::vector<uint8_t>>().size(),
            kDefaultSymSignatureSize);
}

// Derives a symmetric key and then uses it to encrypt/decrypt.
IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       SymKeyDeriveAndEncryptDecrypt) {
  const TokenId token_id = GetParam().token_id;

  base::test::TestFuture<std::vector<uint8_t>, Status> generate_key_waiter;
  platform_keys_service()->GenerateSymKey(
      token_id, kSymId, kDefaultSymKeySize,
      chromeos::platform_keys::SymKeyType::kSp800Kdf,
      generate_key_waiter.GetCallback());
  EXPECT_EQ(generate_key_waiter.Get<Status>(), Status::kSuccess);
  EXPECT_EQ(kSymId, generate_key_waiter.Get<std::vector<uint8_t>>());

  base::test::TestFuture<std::vector<uint8_t>, Status> derive_waiter;
  platform_keys_service()->DeriveSymKey(
      token_id, kSymId, kSymId2, kTestingData, kTestingData,
      chromeos::platform_keys::SymKeyType::kAesCbc,
      derive_waiter.GetCallback());
  EXPECT_EQ(derive_waiter.Get<Status>(), Status::kSuccess);
  EXPECT_EQ(kSymId2, derive_waiter.Get<std::vector<uint8_t>>());

  const std::vector<uint8_t> kInitVec(/*count=*/16, /*value=*/0);
  base::test::TestFuture<std::vector<uint8_t>, Status> encrypt_waiter;
  platform_keys_service()->EncryptAES(token_id, kTestingData, kSymId2,
                                      "AES-CBC", kInitVec,
                                      encrypt_waiter.GetCallback());
  EXPECT_EQ(encrypt_waiter.Get<Status>(), Status::kSuccess);

  std::vector<uint8_t> encrypted_data =
      encrypt_waiter.Get<std::vector<uint8_t>>();
  // Encrypted data's length should be divisible by 16.
  EXPECT_TRUE(encrypted_data.size() % 16 == 0);

  // Decrypting the resulting encrypted data to see if it matches the original
  // data.
  base::test::TestFuture<std::vector<uint8_t>, Status> decrypt_waiter;
  platform_keys_service()->DecryptAES(token_id, encrypted_data, kSymId2,
                                      "AES-CBC", kInitVec,
                                      decrypt_waiter.GetCallback());
  EXPECT_EQ(decrypt_waiter.Get<Status>(), Status::kSuccess);
  EXPECT_EQ(kTestingData, decrypt_waiter.Get<std::vector<uint8_t>>());
}

INSTANTIATE_TEST_SUITE_P(
    AllSupportedProfilesAndTokensTypes,
    PlatformKeysServicePerTokenBrowserTest,
    ::testing::Values(TestConfigPerToken{ProfileToUse::kSigninProfile,
                                         TokenId::kSystem},
                      TestConfigPerToken{ProfileToUse::kUnaffiliatedUserProfile,
                                         TokenId::kUser},
                      TestConfigPerToken{ProfileToUse::kAffiliatedUserProfile,
                                         TokenId::kSystem},
                      TestConfigPerToken{ProfileToUse::kAffiliatedUserProfile,
                                         TokenId::kUser}));

// PlatformKeysServicePerUnavailableTokenBrowserTest is essentially the same as
// PlatformKeysServicePerTokenBrowserTest but contains different test cases
// (testing that the token is not available) and runs on a different set of
// (profile, token) pairs accordingly.
using PlatformKeysServicePerUnavailableTokenBrowserTest =
    PlatformKeysServicePerTokenBrowserTest;

// Uses GenerateRSAKey as an example operation that should fail because the
// token is not available.
IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerUnavailableTokenBrowserTest,
                       GenerateRsa) {
  const unsigned int kKeySize = 2048;

  const TokenId token_id = GetParam().token_id;
  base::test::TestFuture<std::vector<uint8_t>, Status> generate_key_waiter;
  platform_keys_service()->GenerateRSAKey(token_id, kKeySize,
                                          /*sw_backed=*/false,
                                          generate_key_waiter.GetCallback());
  ASSERT_TRUE(generate_key_waiter.Wait());
  EXPECT_NE(generate_key_waiter.Get<Status>(), Status::kSuccess);
}

IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerUnavailableTokenBrowserTest,
                       IsKeyOnToken) {
  const TokenId token_id = GetParam().token_id;

  std::vector<uint8_t> some_key = {1, 2, 3, 4, 5};

  test_util::IsKeyOnTokenExecutionWaiter is_key_on_token_waiter;
  platform_keys_service()->IsKeyOnToken(token_id, std::move(some_key),
                                        is_key_on_token_waiter.GetCallback());
  ASSERT_TRUE(is_key_on_token_waiter.Wait());

  EXPECT_NE(is_key_on_token_waiter.status(), Status::kSuccess);
  EXPECT_FALSE(is_key_on_token_waiter.on_slot().has_value());
}

INSTANTIATE_TEST_SUITE_P(
    AllSupportedProfilesAndTokensTypes,
    PlatformKeysServicePerUnavailableTokenBrowserTest,
    ::testing::Values(TestConfigPerToken{ProfileToUse::kSigninProfile,
                                         TokenId::kUser},
                      TestConfigPerToken{ProfileToUse::kUnaffiliatedUserProfile,
                                         TokenId::kSystem}));
}  // namespace ash::platform_keys
