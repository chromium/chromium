// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>

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
#include "device/fido/features.h"
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

constexpr std::string_view kRpId = "example.com";
constexpr std::array<uint8_t, 3> kUserId{1, 2, 3};
constexpr size_t kSecurityDomainSecretLength = 32;

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
  AssertPasskey(std::string_view rp_id, std::string_view credential_id) {
    base::test::TestFuture<PasskeyAssertionResultPtr> future;
    auto request = PasskeyAssertionRequest::New();
    request->rp_id = rp_id;
    request->credential_id = {credential_id.begin(), credential_id.end()};
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

// The service should return an error if a GetAssertion request is performed
// without a security domain secret.
// TODO(crbug.com/1223853): Implement domain secret recovery.
TEST_F(PasskeyAuthenticatorServiceAshTest, GetAssertionWithoutDomainSecret) {
  ASSERT_TRUE(trusted_vault_client_.GetStoredKeys(account_info_.gaia).empty());
  auto result = AssertPasskey(kRpId, "unknown id");
  EXPECT_EQ(result.error(),
            PasskeyAssertionError::kSecurityDomainSecretUnavailable);
}

TEST_F(PasskeyAuthenticatorServiceAshTest, GetAssertionUnknownCredential) {
  InjectTrustedVaultKey();
  sync_pb::WebauthnCredentialSpecifics credential = InjectCredential(kRpId);
  auto result = AssertPasskey(kRpId, "unknown id");
  EXPECT_EQ(result.error(), PasskeyAssertionError::kCredentialNotFound);
}

TEST_F(PasskeyAuthenticatorServiceAshTest, GetAssertion) {
  InjectTrustedVaultKey();
  sync_pb::WebauthnCredentialSpecifics credential = InjectCredential(kRpId);
  auto result = AssertPasskey(kRpId, credential.credential_id());
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result.value()->signature.empty());
}

}  // namespace

}  // namespace ash
