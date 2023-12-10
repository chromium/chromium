// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/rand_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/passkeys/passkey_authenticator_service_ash.h"
#include "chromeos/crosapi/mojom/passkeys.mojom.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/trusted_vault/test/fake_trusted_vault_client.h"
#include "components/webauthn/core/browser/passkey_model_utils.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "crypto/ec_private_key.h"
#include "crypto/random.h"
#include "crypto/sha2.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/features.h"
#include "device/fido/public_key.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using crosapi::mojom::AccountKey;
using crosapi::mojom::AccountType;
using crosapi::mojom::PasskeyAssertionError;
using crosapi::mojom::PasskeyAssertionRequest;
using crosapi::mojom::PasskeyAssertionRequestPtr;
using crosapi::mojom::PasskeyAssertionResponse;
using crosapi::mojom::PasskeyAssertionResponsePtr;
using crosapi::mojom::PasskeyAssertionResultPtr;
using crosapi::mojom::PasskeyCreationError;
using crosapi::mojom::PasskeyCreationRequest;
using crosapi::mojom::PasskeyCreationRequestPtr;
using crosapi::mojom::PasskeyCreationResponse;
using crosapi::mojom::PasskeyCreationResponsePtr;
using crosapi::mojom::PasskeyCreationResultPtr;
using testing::ElementsAreArray;

constexpr std::string_view kRpId = "example.com";
constexpr std::array<uint8_t, 32> kClientDataHash{
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41};
constexpr std::array<uint8_t, 3> kUserId{1, 2, 3};
constexpr std::array<uint8_t, 3> kDummyCredentialId{1, 2, 3};
constexpr size_t kSecurityDomainSecretLength = 32;

// The expected authenticatorData
// (https://w3c.github.io/webauthn/#authenticator-data) produced by the
// authenticatior for RP ID `example.com`, and with (UP|UV|BE|BS) flags byte,
// zero signature counter, and absent attestedCredentialData and extensions.
constexpr std::array<uint8_t, 37> kExpectedAuthenticatorDataForAssert = {
    0xa3, 0x79, 0xa6, 0xf6, 0xee, 0xaf, 0xb9, 0xa5, 0x5e, 0x37,
    0x8c, 0x11, 0x80, 0x34, 0xe2, 0x75, 0x1e, 0x68, 0x2f, 0xab,
    0x9f, 0x2d, 0x30, 0xab, 0x13, 0xd2, 0x12, 0x55, 0x86, 0xce,
    0x19, 0x47, 0x1d, 0x00, 0x00, 0x00, 0x00};

MATCHER(IsOk, "") {
  if (arg.has_value()) {
    return true;
  }
  *result_listener << "is error: " << arg.error();
  return false;
}

MATCHER_P(IsError, expected, "") {
  if (arg.has_value()) {
    *result_listener << "no error";
    return false;
  }
  if (arg.error() != expected) {
    *result_listener << "actual error: " << arg.error();
    return false;
  }
  return true;
}

sync_pb::WebauthnCredentialSpecifics NewPasskey(
    base::span<const uint8_t> domain_secret) {
  sync_pb::WebauthnCredentialSpecifics specifics;
  specifics.set_sync_id(base::RandBytesAsString(16));
  specifics.set_credential_id(base::RandBytesAsString(16));
  specifics.set_rp_id(kRpId.data());
  specifics.set_user_id({kUserId.begin(), kUserId.end()});
  specifics.set_creation_time(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  sync_pb::WebauthnCredentialSpecifics_Encrypted encrypted_pb;
  auto ec_key = crypto::ECPrivateKey::Create();
  std::vector<uint8_t> private_key_pkcs8;
  CHECK(ec_key->ExportPrivateKey(&private_key_pkcs8));
  encrypted_pb.set_private_key(
      {private_key_pkcs8.begin(), private_key_pkcs8.end()});
  CHECK(webauthn::passkey_model_utils::EncryptWebauthnCredentialSpecificsData(
      domain_secret, encrypted_pb, &specifics));

  return specifics;
}

class PasskeyAuthenticatorServiceAshTest : public testing::Test {
 protected:
  PasskeyAuthenticatorServiceAshTest() {
    account_info_ = identity_test_env_.MakePrimaryAccountAvailable(
        "example@gmail.com", signin::ConsentLevel::kSignin);
    passkey_authenticator_ = std::make_unique<PasskeyAuthenticatorServiceAsh>(
        account_info_, &passkey_model_, &trusted_vault_client_);
    passkey_authenticator_->BindReceiver(
        passkey_authenticator_remote_.BindNewPipeAndPassReceiver());
    crypto::RandBytes(base::make_span(security_domain_secret_));
  }

  void InjectTrustedVaultKey() {
    const std::vector<std::vector<uint8_t>> keys = {
        {security_domain_secret_.begin(), security_domain_secret_.end()}};
    trusted_vault_client_.StoreKeys(account_info_.gaia, keys,
                                    /*last_key_version=*/1);
  }

  sync_pb::WebauthnCredentialSpecifics InjectCredential(
      std::string_view rp_id) {
    sync_pb::WebauthnCredentialSpecifics specifics =
        NewPasskey(security_domain_secret_);
    passkey_model_.AddNewPasskeyForTesting(specifics);
    return specifics;
  }

  base::expected<PasskeyAssertionResponsePtr, PasskeyAssertionError>
  AssertPasskey(std::string_view rp_id,
                base::span<const uint8_t> credential_id) {
    base::test::TestFuture<PasskeyAssertionResultPtr> future;
    auto request = PasskeyAssertionRequest::New();
    request->rp_id = rp_id;
    request->credential_id = {credential_id.begin(), credential_id.end()};
    request->client_data_hash = {kClientDataHash.begin(),
                                 kClientDataHash.end()};
    passkey_authenticator_remote_->Assert(
        AccountKey::New(account_info_.gaia, AccountType::kGaia),
        std::move(request), future.GetCallback());
    // Ensure all calls to `trusted_vault_client_ are able to complete.
    passkey_authenticator_remote_.FlushForTesting();
    trusted_vault_client_.CompleteAllPendingRequests();
    PasskeyAssertionResultPtr result = future.Take();
    if (result->is_error()) {
      return base::unexpected(result->get_error());
    }
    CHECK(result->is_response());
    return std::move(result->get_response());
  }

  base::expected<PasskeyCreationResponsePtr, PasskeyCreationError>
  CreatePasskey(std::string_view rp_id, base::span<const uint8_t> user_id) {
    auto request = PasskeyCreationRequest::New();
    request->rp_id = rp_id;
    request->user_id = {user_id.begin(), user_id.end()};
    return CreatePasskey(std::move(request));
  }

  base::expected<PasskeyCreationResponsePtr, PasskeyCreationError>
  CreatePasskey(PasskeyCreationRequestPtr request) {
    base::test::TestFuture<PasskeyCreationResultPtr> future;
    passkey_authenticator_remote_->Create(
        AccountKey::New(account_info_.gaia, AccountType::kGaia),
        std::move(request), future.GetCallback());
    // Ensure all calls to `trusted_vault_client_ are able to complete.
    passkey_authenticator_remote_.FlushForTesting();
    trusted_vault_client_.CompleteAllPendingRequests();
    PasskeyCreationResultPtr result = future.Take();
    if (result->is_error()) {
      return base::unexpected(result->get_error());
    }
    CHECK(result->is_response());
    return std::move(result->get_response());
  }

  std::unique_ptr<crypto::ECPrivateKey> DecryptPasskeyPrivateKey(
      const sync_pb::WebauthnCredentialSpecifics& passkey) {
    sync_pb::WebauthnCredentialSpecifics_Encrypted encrypted_data;
    CHECK(webauthn::passkey_model_utils::DecryptWebauthnCredentialSpecificsData(
        security_domain_secret_, passkey, &encrypted_data));
    auto private_key = crypto::ECPrivateKey::CreateFromPrivateKeyInfo(
        base::as_byte_span(encrypted_data.private_key()));
    CHECK(private_key);
    return private_key;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_{device::kChromeOsPasskeys};

  std::unique_ptr<PasskeyAuthenticatorServiceAsh> passkey_authenticator_;
  mojo::Remote<crosapi::mojom::PasskeyAuthenticator>
      passkey_authenticator_remote_;

  signin::IdentityTestEnvironment identity_test_env_;
  AccountInfo account_info_;
  webauthn::TestPasskeyModel passkey_model_;
  trusted_vault::FakeTrustedVaultClient trusted_vault_client_;

  std::array<uint8_t, kSecurityDomainSecretLength> security_domain_secret_;
};

TEST_F(PasskeyAuthenticatorServiceAshTest, AssertPasskey) {
  InjectTrustedVaultKey();
  sync_pb::WebauthnCredentialSpecifics credential = InjectCredential(kRpId);
  auto result =
      AssertPasskey(kRpId, base::as_byte_span(credential.credential_id()));
  ASSERT_THAT(result, IsOk());
  EXPECT_FALSE(result.value()->signature.empty());
  EXPECT_TRUE(result.value()->authenticator_data);
  EXPECT_THAT(*result.value()->authenticator_data,
              ElementsAreArray(kExpectedAuthenticatorDataForAssert));
}

// The service should return an error if a Assert request is performed
// without a security domain secret.
// TODO(crbug.com/1223853): Implement domain secret recovery.
TEST_F(PasskeyAuthenticatorServiceAshTest, AssertPasskeyWithoutDomainSecret) {
  ASSERT_TRUE(trusted_vault_client_.GetStoredKeys(account_info_.gaia).empty());
  EXPECT_THAT(AssertPasskey(kRpId, kDummyCredentialId),
              IsError(PasskeyAssertionError::kSecurityDomainSecretUnavailable));
}

TEST_F(PasskeyAuthenticatorServiceAshTest, AssertPasskeyWithUnknownCredential) {
  InjectTrustedVaultKey();
  sync_pb::WebauthnCredentialSpecifics credential = InjectCredential(kRpId);
  EXPECT_THAT(AssertPasskey(kRpId, kDummyCredentialId),
              IsError(PasskeyAssertionError::kCredentialNotFound));
}

TEST_F(PasskeyAuthenticatorServiceAshTest, CreatePasskey) {
  InjectTrustedVaultKey();
  auto create_result = CreatePasskey(kRpId, kUserId);
  ASSERT_THAT(create_result, IsOk());
  const crosapi::mojom::PasskeyCreationResponsePtr& create_response =
      create_result.value();
  ASSERT_EQ(passkey_model_.GetAllPasskeys().size(), 1u);
  const sync_pb::WebauthnCredentialSpecifics passkey =
      passkey_model_.GetAllPasskeys().front();
  auto authenticator_data = device::AuthenticatorData::DecodeAuthenticatorData(
      create_response->authenticator_data);
  ASSERT_TRUE(authenticator_data);
  EXPECT_THAT(authenticator_data->application_parameter(),
              ElementsAreArray(crypto::SHA256Hash(base::as_byte_span(kRpId))));
  EXPECT_EQ(authenticator_data->flags(), 0x5d);
  EXPECT_THAT(authenticator_data->counter(),
              ElementsAreArray(std::array<uint8_t, 4>{0, 0, 0, 0}));
  EXPECT_THAT(authenticator_data->attested_data()->credential_id(),
              ElementsAreArray(base::as_byte_span(passkey.credential_id())));
  EXPECT_EQ(base::HexEncode(authenticator_data->attested_data()->aaguid()),
            "EA9B8D664D011D213CE4B6B48CB575D4");
  auto private_key = DecryptPasskeyPrivateKey(passkey);
  std::vector<uint8_t> expected_public_key;
  ASSERT_TRUE(private_key->ExportPublicKey(&expected_public_key));
  EXPECT_EQ(authenticator_data->attested_data()->public_key()->algorithm, -7);
  EXPECT_THAT(*authenticator_data->attested_data()->public_key()->der_bytes,
              ElementsAreArray(expected_public_key));
}

// TODO(crbug.com/1223853): Implement domain secret recovery.
TEST_F(PasskeyAuthenticatorServiceAshTest, CreatePasskeyWithoutDomainSecret) {
  ASSERT_TRUE(trusted_vault_client_.GetStoredKeys(account_info_.gaia).empty());
  auto result = CreatePasskey(kRpId, kUserId);
  EXPECT_THAT(CreatePasskey(kRpId, kUserId),
              IsError(PasskeyCreationError::kSecurityDomainSecretUnavailable));
}

}  // namespace

}  // namespace ash
