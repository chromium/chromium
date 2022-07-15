// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/cryptohome/cryptohome_util.h"

#include <string>

#include "ash/components/cryptohome/cryptohome_parameters.h"
#include "ash/components/login/auth/public/challenge_response_key.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cryptohome {

using ::chromeos::ChallengeResponseKey;

constexpr char kKeyLabel[] = "key_label";

TEST(CryptohomeUtilTest, CreateAuthorizationRequestEmptyLabel) {
  const std::string kExpectedSecret = "secret";

  const AuthorizationRequest auth_request =
      CreateAuthorizationRequest(std::string(), kExpectedSecret);

  EXPECT_FALSE(auth_request.key().data().has_label());
  EXPECT_EQ(auth_request.key().secret(), kExpectedSecret);
}

TEST(CryptohomeUtilTest, CreateAuthorizationRequestWithLabel) {
  const std::string kExpectedLabel = "some_label";
  const std::string kExpectedSecret = "some_secret";

  const AuthorizationRequest auth_request =
      CreateAuthorizationRequest(kExpectedLabel, kExpectedSecret);

  EXPECT_EQ(auth_request.key().data().label(), kExpectedLabel);
  EXPECT_EQ(auth_request.key().secret(), kExpectedSecret);
}

TEST(CryptohomeUtilTest,
     CreateAuthorizationRequestFromKeyDefPasswordEmptyLabel) {
  const std::string kExpectedSecret = "secret";

  const AuthorizationRequest auth_request =
      CreateAuthorizationRequestFromKeyDef(KeyDefinition::CreateForPassword(
          kExpectedSecret, std::string() /* label */, PRIV_DEFAULT));

  EXPECT_FALSE(auth_request.key().data().has_label());
  EXPECT_EQ(auth_request.key().secret(), kExpectedSecret);
}

TEST(CryptohomeUtilTest,
     CreateAuthorizationRequestFromKeyDefPasswordWithLabel) {
  const std::string kExpectedSecret = "secret";

  const AuthorizationRequest auth_request =
      CreateAuthorizationRequestFromKeyDef(KeyDefinition::CreateForPassword(
          kExpectedSecret, kKeyLabel, PRIV_DEFAULT));

  EXPECT_EQ(auth_request.key().data().label(), kKeyLabel);
  EXPECT_EQ(auth_request.key().secret(), kExpectedSecret);
}

TEST(CryptohomeUtilTest,
     CreateAuthorizationRequestFromKeyDefChallengeResponse) {
  using Algorithm = ChallengeResponseKey::SignatureAlgorithm;
  const std::string kKeySpki = "spki";
  const Algorithm kKeyAlgorithm = Algorithm::kRsassaPkcs1V15Sha1;
  const ChallengeSignatureAlgorithm kKeyAlgorithmProto =
      CHALLENGE_RSASSA_PKCS1_V1_5_SHA1;

  ChallengeResponseKey challenge_response_key;
  challenge_response_key.set_public_key_spki_der(kKeySpki);
  challenge_response_key.set_signature_algorithms({kKeyAlgorithm});
  const KeyDefinition key_def = KeyDefinition::CreateForChallengeResponse(
      {challenge_response_key}, kKeyLabel, PRIV_DEFAULT);

  const AuthorizationRequest auth_request =
      CreateAuthorizationRequestFromKeyDef(key_def);

  EXPECT_FALSE(auth_request.key().has_secret());
  EXPECT_EQ(auth_request.key().data().type(),
            KeyData::KEY_TYPE_CHALLENGE_RESPONSE);
  EXPECT_EQ(auth_request.key().data().label(), kKeyLabel);
  ASSERT_EQ(auth_request.key().data().challenge_response_key_size(), 1);
  EXPECT_EQ(
      auth_request.key().data().challenge_response_key(0).public_key_spki_der(),
      kKeySpki);
  ASSERT_EQ(auth_request.key()
                .data()
                .challenge_response_key(0)
                .signature_algorithm_size(),
            1);
  EXPECT_EQ(
      auth_request.key().data().challenge_response_key(0).signature_algorithm(
          0),
      kKeyAlgorithmProto);
}

TEST(CryptohomeUtilTest, KeyDefinitionToKeyType) {
  Key key;

  KeyDefinitionToKey(KeyDefinition(), &key);

  EXPECT_EQ(key.data().type(), KeyData::KEY_TYPE_PASSWORD);
}

TEST(CryptohomeUtilTest, KeyDefinitionToKeySecret) {
  const std::string kExpectedSecret = "my_dog_ate_my_homework";
  KeyDefinition key_def;
  key_def.secret = kExpectedSecret;
  Key key;

  KeyDefinitionToKey(key_def, &key);

  EXPECT_EQ(key.secret(), kExpectedSecret);
}

TEST(CryptohomeUtilTest, KeyDefinitionToKeyLabel) {
  const std::string kExpectedLabel = "millenials hate labels";
  KeyDefinition key_def;
  key_def.label = kExpectedLabel;
  Key key;

  KeyDefinitionToKey(key_def, &key);

  EXPECT_EQ(key.data().label(), kExpectedLabel);
}

TEST(CryptohomeUtilTest, KeyDefinitionToKeyNonpositiveRevision) {
  KeyDefinition key_def;
  key_def.revision = -1;
  Key key;

  KeyDefinitionToKey(key_def, &key);

  EXPECT_EQ(key.data().revision(), 0);
}

TEST(CryptohomeUtilTest, KeyDefinitionToKeyPositiveRevision) {
  constexpr int kExpectedRevision = 10;
  KeyDefinition key_def;
  key_def.revision = kExpectedRevision;
  Key key;

  KeyDefinitionToKey(key_def, &key);

  EXPECT_EQ(key.data().revision(), kExpectedRevision);
}

TEST(CryptohomeUtilTest, KeyDefinitionToKeyDefaultPrivileges) {
  KeyDefinition key_def;
  Key key;

  KeyDefinitionToKey(key_def, &key);
  KeyPrivileges privileges = key.data().privileges();

  EXPECT_TRUE(privileges.add());
  EXPECT_TRUE(privileges.remove());
  EXPECT_TRUE(privileges.update());
}

TEST(CryptohomeUtilTest, KeyDefinitionToKeyAddPrivileges) {
  KeyDefinition key_def;
  key_def.privileges = PRIV_ADD;
  Key key;

  KeyDefinitionToKey(key_def, &key);
  KeyPrivileges privileges = key.data().privileges();

  EXPECT_TRUE(privileges.add());
  EXPECT_FALSE(privileges.remove());
  EXPECT_FALSE(privileges.update());
}

TEST(CryptohomeUtilTest, KeyDefinitionToKeyRemovePrivileges) {
  KeyDefinition key_def;
  key_def.privileges = PRIV_REMOVE;
  Key key;

  KeyDefinitionToKey(key_def, &key);
  KeyPrivileges privileges = key.data().privileges();

  EXPECT_FALSE(privileges.add());
  EXPECT_TRUE(privileges.remove());
  EXPECT_FALSE(privileges.update());
}

TEST(CryptohomeUtilTest, KeyDefinitionToKeyUpdatePrivileges) {
  KeyDefinition key_def;
  key_def.privileges = PRIV_MIGRATE;
  Key key;

  KeyDefinitionToKey(key_def, &key);
  KeyPrivileges privileges = key.data().privileges();

  EXPECT_FALSE(privileges.add());
  EXPECT_FALSE(privileges.remove());
  EXPECT_TRUE(privileges.update());
}

TEST(CryptohomeUtilTest, KeyDefinitionToKeyAllPrivileges) {
  KeyDefinition key_def;
  key_def.privileges = PRIV_DEFAULT;
  Key key;

  KeyDefinitionToKey(key_def, &key);
  KeyPrivileges privileges = key.data().privileges();

  EXPECT_TRUE(privileges.add());
  EXPECT_TRUE(privileges.remove());
  EXPECT_TRUE(privileges.update());
}

// Test the KeyDefinitionToKey() function against the KeyDefinition struct of
// the |TYPE_CHALLENGE_RESPONSE| type.
TEST(CryptohomeUtilTest, KeyDefinitionToKey_ChallengeResponse) {
  using Algorithm = ChallengeResponseKey::SignatureAlgorithm;
  const int kPrivileges = 0;
  const std::string kKey1Spki = "spki1";
  const Algorithm kKey1Algorithm = Algorithm::kRsassaPkcs1V15Sha1;
  const ChallengeSignatureAlgorithm kKey1AlgorithmProto =
      CHALLENGE_RSASSA_PKCS1_V1_5_SHA1;
  const std::string kKey2Spki = "spki2";
  const Algorithm kKey2Algorithm1 = Algorithm::kRsassaPkcs1V15Sha512;
  const ChallengeSignatureAlgorithm kKey2Algorithm1Proto =
      CHALLENGE_RSASSA_PKCS1_V1_5_SHA512;
  const Algorithm kKey2Algorithm2 = Algorithm::kRsassaPkcs1V15Sha256;
  const ChallengeSignatureAlgorithm kKey2Algorithm2Proto =
      CHALLENGE_RSASSA_PKCS1_V1_5_SHA256;

  ChallengeResponseKey challenge_response_key1;
  challenge_response_key1.set_public_key_spki_der(kKey1Spki);
  challenge_response_key1.set_signature_algorithms({kKey1Algorithm});
  ChallengeResponseKey challenge_response_key2;
  challenge_response_key2.set_public_key_spki_der(kKey2Spki);
  challenge_response_key2.set_signature_algorithms(
      {kKey2Algorithm1, kKey2Algorithm2});
  const KeyDefinition key_def = KeyDefinition::CreateForChallengeResponse(
      {challenge_response_key1, challenge_response_key2}, kKeyLabel,
      kPrivileges);
  Key key;

  KeyDefinitionToKey(key_def, &key);

  EXPECT_FALSE(key.has_secret());
  EXPECT_EQ(key.data().type(), KeyData::KEY_TYPE_CHALLENGE_RESPONSE);
  EXPECT_EQ(key.data().label(), kKeyLabel);
  ASSERT_EQ(key.data().challenge_response_key_size(), 2);
  EXPECT_EQ(key.data().challenge_response_key(0).public_key_spki_der(),
            kKey1Spki);
  ASSERT_EQ(key.data().challenge_response_key(0).signature_algorithm_size(), 1);
  EXPECT_EQ(key.data().challenge_response_key(0).signature_algorithm(0),
            kKey1AlgorithmProto);
  EXPECT_EQ(key.data().challenge_response_key(1).public_key_spki_der(),
            kKey2Spki);
  ASSERT_EQ(key.data().challenge_response_key(1).signature_algorithm_size(), 2);
  EXPECT_EQ(key.data().challenge_response_key(1).signature_algorithm(0),
            kKey2Algorithm1Proto);
  EXPECT_EQ(key.data().challenge_response_key(1).signature_algorithm(1),
            kKey2Algorithm2Proto);
}

}  // namespace cryptohome
