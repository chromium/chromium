// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/scoped_policy_update.h"
#include "chrome/browser/chromeos/login/test/user_policy_mixin.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/scoped_test_system_nss_key_slot_mixin.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/login/auth/user_context.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "crypto/nss_key_util.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/signature_verifier.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/constants/pkcs11_custom_attributes.h"

namespace chromeos {
namespace platform_keys {
namespace {

constexpr char kTestUserEmail[] = "test@example.com";
constexpr char kTestAffiliationId[] = "test_affiliation_id";

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

// Softoken NSS PKCS11 module (used for testing) allows only predefined key
// attributes to be set and retrieved. Chaps supports setting and retrieving
// custom attributes.
// This helper is created to allow setting and retrieving attributes
// supported by PlatformKeysService. For the lifetime of an instance of this
// helper, PlatformKeysService will be configured to use softoken key attribute
// mapping for the lifetime of the helper.
class ScopedSoftokenAttrsMapping {
 public:
  explicit ScopedSoftokenAttrsMapping(
      PlatformKeysService* platform_keys_service);
  ScopedSoftokenAttrsMapping(const ScopedSoftokenAttrsMapping& other) = delete;
  ScopedSoftokenAttrsMapping& operator=(
      const ScopedSoftokenAttrsMapping& other) = delete;
  ~ScopedSoftokenAttrsMapping();

 private:
  PlatformKeysService* const platform_keys_service_;
};

ScopedSoftokenAttrsMapping::ScopedSoftokenAttrsMapping(
    PlatformKeysService* platform_keys_service)
    : platform_keys_service_(platform_keys_service) {
  platform_keys_service_->SetMapToSoftokenAttrsForTesting(true);
}

ScopedSoftokenAttrsMapping::~ScopedSoftokenAttrsMapping() {
  DCHECK(platform_keys_service_);
  platform_keys_service_->SetMapToSoftokenAttrsForTesting(false);
}

// A helper that waits until execution of an asynchronous PlatformKeysService
// operation has finished and provides access to the results.
// Note: all PlatformKeysService operations have a trailing status argument that
// is filled in case of an error.
template <typename... ResultCallbackArgs>
class ExecutionWaiter {
 public:
  ExecutionWaiter() = default;
  ~ExecutionWaiter() = default;
  ExecutionWaiter(const ExecutionWaiter& other) = delete;
  ExecutionWaiter& operator=(const ExecutionWaiter& other) = delete;

  // Returns the callback to be passed to the PlatformKeysService operation
  // invocation.
  base::RepeatingCallback<void(ResultCallbackArgs... result_callback_args,
                               Status status)>
  GetCallback() {
    return base::BindRepeating(&ExecutionWaiter::OnExecutionDone,
                               weak_ptr_factory_.GetWeakPtr());
  }

  // Waits until the callback returned by GetCallback() has been called.
  void Wait() { run_loop_.Run(); }

  // Returns the status passed to the callback.
  Status status() const {
    EXPECT_TRUE(done_);
    return status_;
  }

 protected:
  // A std::tuple that is capable of storing the arguments passed to the result
  // callback.
  using ResultCallbackArgsTuple =
      std::tuple<std::decay_t<ResultCallbackArgs>...>;

  // Access to the arguments passed to the callback.
  const ResultCallbackArgsTuple& result_callback_args() const {
    EXPECT_TRUE(done_);
    return result_callback_args_;
  }

 private:
  void OnExecutionDone(ResultCallbackArgs... result_callback_args,
                       Status status) {
    EXPECT_FALSE(done_);
    done_ = true;
    result_callback_args_ = ResultCallbackArgsTuple(
        std::forward<ResultCallbackArgs>(result_callback_args)...);
    status_ = status;
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  bool done_ = false;
  ResultCallbackArgsTuple result_callback_args_;
  Status status_ = Status::kSuccess;

  base::WeakPtrFactory<ExecutionWaiter> weak_ptr_factory_{this};
};

// Supports waiting for the result of PlatformKeysService::GetTokens.
class GetTokensExecutionWaiter
    : public ExecutionWaiter<std::unique_ptr<std::vector<TokenId>>> {
 public:
  GetTokensExecutionWaiter() = default;
  ~GetTokensExecutionWaiter() = default;

  const std::unique_ptr<std::vector<TokenId>>& token_ids() const {
    return std::get<0>(result_callback_args());
  }
};

// Supports waiting for the result of the PlatformKeysService::GenerateKey*
// function family.
class GenerateKeyExecutionWaiter : public ExecutionWaiter<const std::string&> {
 public:
  GenerateKeyExecutionWaiter() = default;
  ~GenerateKeyExecutionWaiter() = default;

  const std::string& public_key_spki_der() const {
    return std::get<0>(result_callback_args());
  }
};

// Supports waiting for the result of the PlatformKeysService::Sign* function
// family.
class SignExecutionWaiter : public ExecutionWaiter<const std::string&> {
 public:
  SignExecutionWaiter() = default;
  ~SignExecutionWaiter() = default;

  const std::string& signature() const {
    return std::get<0>(result_callback_args());
  }
};

// Supports waiting for the result of the PlatformKeysService::GetCertificates.
class GetCertificatesExecutionWaiter
    : public ExecutionWaiter<std::unique_ptr<net::CertificateList>> {
 public:
  GetCertificatesExecutionWaiter() = default;
  ~GetCertificatesExecutionWaiter() = default;

  const net::CertificateList& matches() const {
    return *std::get<0>(result_callback_args());
  }
};

// Supports waiting for the result of the
// PlatformKeysService::SetAttributeForKey.
class SetAttributeForKeyExecutionWaiter : public ExecutionWaiter<> {
 public:
  SetAttributeForKeyExecutionWaiter() = default;
  ~SetAttributeForKeyExecutionWaiter() = default;
};

// Supports waiting for the result of the
// PlatformKeysService::GetAttributeForKey.
class GetAttributeForKeyExecutionWaiter
    : public ExecutionWaiter<const base::Optional<std::string>&> {
 public:
  GetAttributeForKeyExecutionWaiter() = default;
  ~GetAttributeForKeyExecutionWaiter() = default;

  const base::Optional<std::string>& attribute_value() const {
    return std::get<0>(result_callback_args());
  }
};

// Supports waiting for the result of the PlatformKeysService::RemoveKey.
class RemoveKeyExecutionWaiter : public ExecutionWaiter<> {
 public:
  RemoveKeyExecutionWaiter() = default;
  ~RemoveKeyExecutionWaiter() = default;
};

class GetAllKeysExecutionWaiter
    : public ExecutionWaiter<std::vector<std::string>> {
 public:
  GetAllKeysExecutionWaiter() = default;
  ~GetAllKeysExecutionWaiter() = default;

  const std::vector<std::string>& public_keys() const {
    return std::get<0>(result_callback_args());
  }
};

class IsKeyOnTokenExecutionWaiter
    : public ExecutionWaiter<base::Optional<bool>> {
 public:
  IsKeyOnTokenExecutionWaiter() = default;
  ~IsKeyOnTokenExecutionWaiter() = default;

  base::Optional<bool> on_slot() const {
    return std::get<0>(result_callback_args());
  }
};

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
      GetNSSCertDatabaseForProfile(
          profile_,
          base::BindRepeating(&PlatformKeysServiceBrowserTestBase::SetUserSlot,
                              base::Unretained(this), loop.QuitClosure()));
      loop.Run();
    }
    ASSERT_TRUE(profile_);

    platform_keys_service_ =
        PlatformKeysServiceFactory::GetForBrowserContext(profile_);
    ASSERT_TRUE(platform_keys_service_);

    scoped_softoken_attrs_mapping_ =
        std::make_unique<ScopedSoftokenAttrsMapping>(platform_keys_service_);
  }

  void TearDownOnMainThread() override {
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();

    // Destroy |scoped_softoken_attrs_mapping_| before |platform_keys_service_|
    // is destroyed.
    scoped_softoken_attrs_mapping_.reset();
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
  std::string GenerateKeyPair(TokenId token_id) {
    const unsigned int kKeySize = 2048;

    GenerateKeyExecutionWaiter generate_key_waiter;
    platform_keys_service()->GenerateRSAKey(token_id, kKeySize,
                                            generate_key_waiter.GetCallback());
    generate_key_waiter.Wait();

    return generate_key_waiter.public_key_spki_der();
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
                        std::string* out_spki_der) {
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
        std::string(reinterpret_cast<const char*>(cert->derPublicKey.data),
                    cert->derPublicKey.len);
  }

 private:
  void SetUserSlot(const base::Closure& done_callback,
                   net::NSSCertDatabase* db) {
    user_slot_ = db->GetPrivateSlot();
    done_callback.Run();
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
  Profile* profile_ = nullptr;
  // Unowned pointer to the PlatformKeysService for |profile_|. Valid after
  // SetUpOnMainThread().
  PlatformKeysService* platform_keys_service_ = nullptr;
  // The private slot for the profile under test. This should be null if the
  // test parameter mandates testing with the sign-in profile.
  crypto::ScopedPK11Slot user_slot_;
  // Owned pointer to a ScopedSoftokenAttrsMapping object that is created after
  // |platform_keys_service_| is valid.
  std::unique_ptr<ScopedSoftokenAttrsMapping> scoped_softoken_attrs_mapping_;
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

// Tests that GetTokens() is callable and returns the expected tokens.
IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerProfileBrowserTest, GetTokens) {
  GetTokensExecutionWaiter get_tokens_waiter;
  platform_keys_service()->GetTokens(get_tokens_waiter.GetCallback());
  get_tokens_waiter.Wait();

  EXPECT_EQ(get_tokens_waiter.status(), Status::kSuccess);
  ASSERT_TRUE(get_tokens_waiter.token_ids());
  EXPECT_THAT(*get_tokens_waiter.token_ids(),
              ::testing::UnorderedElementsAreArray(GetParam().token_ids));
}

// Generates a key pair in tokens accessible from the profile under test and
// retrieves them.
IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerProfileBrowserTest, GetAllKeys) {
  // Generate key pair in every token.
  std::map<TokenId, std::string> token_key_map;
  for (TokenId token_id : GetParam().token_ids) {
    const std::string public_key_spki_der = GenerateKeyPair(token_id);
    ASSERT_FALSE(public_key_spki_der.empty());
    token_key_map[token_id] = public_key_spki_der;
  }

  // Only keys in the requested token should be retrieved.
  for (TokenId token_id : GetParam().token_ids) {
    GetAllKeysExecutionWaiter get_all_keys_waiter;
    platform_keys_service()->GetAllKeys(token_id,
                                        get_all_keys_waiter.GetCallback());
    get_all_keys_waiter.Wait();

    EXPECT_EQ(get_all_keys_waiter.status(), Status::kSuccess);
    std::vector<std::string> public_keys = get_all_keys_waiter.public_keys();
    ASSERT_EQ(public_keys.size(), 1U);
    EXPECT_EQ(public_keys[0], token_key_map[token_id]);
  }
}

// Imports the same key into all tokens. Verifies that key attributes are stored
// per-token and don't leak between tokens.
IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerProfileBrowserTest,
                       KeyAttributesPerToken) {
  // Import the same key pair + cert in every token, remember its SPKI.
  std::string spki_der;
  for (TokenId token_id : GetParam().token_ids) {
    net::ScopedCERTCertificate cert;
    std::string current_spki_der;
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
  std::map<TokenId, std::string> token_to_value;
  for (TokenId token_id : GetParam().token_ids) {
    token_to_value[token_id] =
        base::StringPrintf("test_value_%d", static_cast<int>(token_id));

    // Set key attribute.
    SetAttributeForKeyExecutionWaiter set_attr_waiter;
    platform_keys_service()->SetAttributeForKey(
        token_id, spki_der, kAttributeType, token_to_value[token_id],
        set_attr_waiter.GetCallback());
    set_attr_waiter.Wait();
    EXPECT_EQ(set_attr_waiter.status(), Status::kSuccess);
  }

  // Verify the token-specific attribute value for the key on each token.
  for (TokenId token_id : GetParam().token_ids) {
    // Get key attribute.
    GetAttributeForKeyExecutionWaiter get_attr_waiter;
    platform_keys_service()->GetAttributeForKey(
        token_id, spki_der, kAttributeType, get_attr_waiter.GetCallback());
    get_attr_waiter.Wait();

    EXPECT_EQ(get_attr_waiter.status(), Status::kSuccess);
    ASSERT_TRUE(get_attr_waiter.attribute_value());
    EXPECT_EQ(get_attr_waiter.attribute_value().value(),
              token_to_value[token_id]);
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

  const std::string public_key = GenerateKeyPair(token_id_1);

  IsKeyOnTokenExecutionWaiter is_key_on_token_waiter;
  platform_keys_service()->IsKeyOnToken(token_id_2, public_key,
                                        is_key_on_token_waiter.GetCallback());
  is_key_on_token_waiter.Wait();

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
  const std::string kDataToSign = "test";
  const unsigned int kKeySize = 2048;
  const HashAlgorithm kHashAlgorithm = HASH_ALGORITHM_SHA256;
  const crypto::SignatureVerifier::SignatureAlgorithm kSignatureAlgorithm =
      crypto::SignatureVerifier::RSA_PKCS1_SHA256;

  const TokenId token_id = GetParam().token_id;
  GenerateKeyExecutionWaiter generate_key_waiter;
  platform_keys_service()->GenerateRSAKey(token_id, kKeySize,
                                          generate_key_waiter.GetCallback());
  generate_key_waiter.Wait();
  EXPECT_EQ(generate_key_waiter.status(), Status::kSuccess);

  const std::string public_key_spki_der =
      generate_key_waiter.public_key_spki_der();
  EXPECT_FALSE(public_key_spki_der.empty());

  SignExecutionWaiter sign_waiter;
  platform_keys_service()->SignRSAPKCS1Digest(
      token_id, kDataToSign, public_key_spki_der, kHashAlgorithm,
      sign_waiter.GetCallback());
  sign_waiter.Wait();
  EXPECT_EQ(sign_waiter.status(), Status::kSuccess);

  crypto::SignatureVerifier signature_verifier;
  ASSERT_TRUE(signature_verifier.VerifyInit(
      kSignatureAlgorithm,
      base::as_bytes(base::make_span(sign_waiter.signature())),
      base::as_bytes(base::make_span(public_key_spki_der))));
  signature_verifier.VerifyUpdate(base::as_bytes(base::make_span(kDataToSign)));
  EXPECT_TRUE(signature_verifier.VerifyFinal());
}

IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       SetAndGetKeyAttribute) {
  // The attribute type to be set and retrieved using platform keys service.
  const KeyAttributeType kAttributeType =
      KeyAttributeType::kCertificateProvisioningId;
  const TokenId token_id = GetParam().token_id;
  const std::string kAttributeValue = "test_attr_value";

  // Generate key pair.
  const std::string public_key_spki_der = GenerateKeyPair(token_id);
  ASSERT_FALSE(public_key_spki_der.empty());

  // Set key attribute.
  SetAttributeForKeyExecutionWaiter set_attribute_for_key_execution_waiter;
  platform_keys_service()->SetAttributeForKey(
      token_id, public_key_spki_der, kAttributeType, kAttributeValue,
      set_attribute_for_key_execution_waiter.GetCallback());
  set_attribute_for_key_execution_waiter.Wait();

  // Get key attribute.
  GetAttributeForKeyExecutionWaiter get_attribute_for_key_execution_waiter;
  platform_keys_service()->GetAttributeForKey(
      token_id, public_key_spki_der, kAttributeType,
      get_attribute_for_key_execution_waiter.GetCallback());
  get_attribute_for_key_execution_waiter.Wait();

  EXPECT_EQ(get_attribute_for_key_execution_waiter.status(), Status::kSuccess);
  ASSERT_TRUE(get_attribute_for_key_execution_waiter.attribute_value());
  EXPECT_EQ(get_attribute_for_key_execution_waiter.attribute_value().value(),
            kAttributeValue);
}

// TODO(https://crbug.com/1073515): Add a test for an unset key attribute when
// simulating chaps behavior is possible.

IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       GetKeyAttributeForNonExistingKey) {
  const KeyAttributeType kAttributeType =
      KeyAttributeType::kCertificateProvisioningId;
  const TokenId token_id = GetParam().token_id;
  const std::string kPublicKey = "Non Existing public key";

  // Get key attribute.
  GetAttributeForKeyExecutionWaiter get_attribute_for_key_execution_waiter;
  platform_keys_service()->GetAttributeForKey(
      token_id, kPublicKey, kAttributeType,
      get_attribute_for_key_execution_waiter.GetCallback());
  get_attribute_for_key_execution_waiter.Wait();

  EXPECT_NE(get_attribute_for_key_execution_waiter.status(), Status::kSuccess);
  EXPECT_FALSE(get_attribute_for_key_execution_waiter.attribute_value());
}

IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       SetKeyAttributeForNonExistingKey) {
  const KeyAttributeType kAttributeType =
      KeyAttributeType::kCertificateProvisioningId;
  const TokenId token_id = GetParam().token_id;
  const std::string kAttributeValue = "test";
  const std::string kPublicKey = "Non Existing public key";

  // Set key attribute.
  SetAttributeForKeyExecutionWaiter set_attribute_for_key_execution_waiter;
  platform_keys_service()->SetAttributeForKey(
      token_id, kPublicKey, kAttributeType, kAttributeValue,
      set_attribute_for_key_execution_waiter.GetCallback());
  set_attribute_for_key_execution_waiter.Wait();

  EXPECT_NE(set_attribute_for_key_execution_waiter.status(), Status::kSuccess);
}

IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       RemoveKeyWithNoMatchingCertificates) {
  const TokenId token_id = GetParam().token_id;

  // Generate first key pair.
  const std::string public_key_1 = GenerateKeyPair(token_id);
  ASSERT_FALSE(public_key_1.empty());

  // Generate second key pair.
  const std::string public_key_2 = GenerateKeyPair(token_id);
  ASSERT_FALSE(public_key_2.empty());

  auto public_key_bytes_1 = base::as_bytes(base::make_span(public_key_1));
  auto public_key_bytes_2 = base::as_bytes(base::make_span(public_key_2));
  EXPECT_TRUE(crypto::FindNSSKeyFromPublicKeyInfo(public_key_bytes_1));
  EXPECT_TRUE(crypto::FindNSSKeyFromPublicKeyInfo(public_key_bytes_2));

  RemoveKeyExecutionWaiter remove_key_waiter;
  platform_keys_service()->RemoveKey(token_id, public_key_1,
                                     remove_key_waiter.GetCallback());
  remove_key_waiter.Wait();

  EXPECT_EQ(remove_key_waiter.status(), Status::kSuccess);
  EXPECT_FALSE(crypto::FindNSSKeyFromPublicKeyInfo(public_key_bytes_1));
  EXPECT_TRUE(crypto::FindNSSKeyFromPublicKeyInfo(public_key_bytes_2));
}

IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       RemoveKeyWithMatchingCertificate) {
  const TokenId token_id = GetParam().token_id;

  // Assert that there are no certificates before importing.
  GetCertificatesExecutionWaiter get_certificates_waiter;
  platform_keys_service()->GetCertificates(
      token_id, get_certificates_waiter.GetCallback());
  get_certificates_waiter.Wait();
  ASSERT_EQ(get_certificates_waiter.matches().size(), 0U);

  net::ScopedCERTCertificate cert;
  std::string public_key;
  ASSERT_NO_FATAL_FAILURE(
      ImportCertAndKey(token_id, net::GetTestCertsDirectory(), "client_1.pem",
                       "client_1.pk8", &cert, &public_key));

  // Assert that the certificate is imported correctly.
  ASSERT_TRUE(cert.get());
  GetCertificatesExecutionWaiter get_certificates_waiter_2;
  platform_keys_service()->GetCertificates(
      token_id, get_certificates_waiter_2.GetCallback());
  get_certificates_waiter_2.Wait();
  ASSERT_EQ(get_certificates_waiter_2.matches().size(), 1U);

  auto public_key_bytes = base::as_bytes(base::make_span(public_key));
  EXPECT_TRUE(crypto::FindNSSKeyFromPublicKeyInfo(public_key_bytes));

  // Try Removing the key pair.
  RemoveKeyExecutionWaiter remove_key_waiter;
  platform_keys_service()->RemoveKey(token_id, public_key,
                                     remove_key_waiter.GetCallback());
  remove_key_waiter.Wait();
  EXPECT_NE(remove_key_waiter.status(), Status::kSuccess);

  // Assert that the certificate is not removed.
  GetCertificatesExecutionWaiter get_certificates_waiter_3;
  platform_keys_service()->GetCertificates(
      token_id, get_certificates_waiter_3.GetCallback());
  get_certificates_waiter_3.Wait();

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
  GetAllKeysExecutionWaiter get_all_keys_waiter;
  platform_keys_service()->GetAllKeys(token_id,
                                      get_all_keys_waiter.GetCallback());
  get_all_keys_waiter.Wait();

  EXPECT_EQ(get_all_keys_waiter.status(), Status::kSuccess);
  std::vector<std::string> public_keys = get_all_keys_waiter.public_keys();
  EXPECT_TRUE(public_keys.empty());
}

IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest, IsKeyOnToken) {
  const TokenId token_id = GetParam().token_id;
  const std::string public_key = GenerateKeyPair(token_id);

  IsKeyOnTokenExecutionWaiter is_key_on_token_waiter;
  platform_keys_service()->IsKeyOnToken(token_id, public_key,
                                        is_key_on_token_waiter.GetCallback());
  is_key_on_token_waiter.Wait();

  EXPECT_EQ(is_key_on_token_waiter.status(), Status::kSuccess);
  ASSERT_TRUE(is_key_on_token_waiter.on_slot().has_value());
  EXPECT_TRUE(is_key_on_token_waiter.on_slot().value());
}

IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerTokenBrowserTest,
                       IsKeyOnTokenWhenNoKeysGenerated) {
  const TokenId token_id = GetParam().token_id;

  IsKeyOnTokenExecutionWaiter is_key_on_token_waiter;
  platform_keys_service()->IsKeyOnToken(token_id, "test_public_key",
                                        is_key_on_token_waiter.GetCallback());
  is_key_on_token_waiter.Wait();

  EXPECT_EQ(is_key_on_token_waiter.status(), Status::kSuccess);
  ASSERT_TRUE(is_key_on_token_waiter.on_slot().has_value());
  EXPECT_FALSE(is_key_on_token_waiter.on_slot().value());
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
  GenerateKeyExecutionWaiter generate_key_waiter;
  platform_keys_service()->GenerateRSAKey(token_id, kKeySize,
                                          generate_key_waiter.GetCallback());
  generate_key_waiter.Wait();
  EXPECT_NE(generate_key_waiter.status(), Status::kSuccess);
}

IN_PROC_BROWSER_TEST_P(PlatformKeysServicePerUnavailableTokenBrowserTest,
                       IsKeyOnToken) {
  const TokenId token_id = GetParam().token_id;

  IsKeyOnTokenExecutionWaiter is_key_on_token_waiter;
  platform_keys_service()->IsKeyOnToken(token_id, "test_public_key",
                                        is_key_on_token_waiter.GetCallback());
  is_key_on_token_waiter.Wait();

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
}  // namespace platform_keys
}  // namespace chromeos
