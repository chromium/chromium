// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/base64.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/sync_base_switches.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace encryption_helper {

bool GetServerNigori(fake_server::FakeServer* fake_server,
                     sync_pb::NigoriSpecifics* nigori) {
  std::vector<sync_pb::SyncEntity> entity_list =
      fake_server->GetPermanentSyncEntitiesByModelType(syncer::NIGORI);
  if (entity_list.size() != 1U) {
    return false;
  }

  *nigori = entity_list[0].specifics().nigori();
  return true;
}

std::unique_ptr<syncer::Cryptographer>
InitCustomPassphraseCryptographerFromNigori(
    const sync_pb::NigoriSpecifics& nigori,
    const std::string& passphrase) {
  auto cryptographer = std::make_unique<syncer::DirectoryCryptographer>();
  sync_pb::EncryptedData keybag = nigori.encryption_keybag();
  cryptographer->SetPendingKeys(keybag);

  std::string decoded_salt;
  switch (syncer::ProtoKeyDerivationMethodToEnum(
      nigori.custom_passphrase_key_derivation_method())) {
    case syncer::KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
      EXPECT_TRUE(cryptographer->DecryptPendingKeys(
          {syncer::KeyDerivationParams::CreateForPbkdf2(), passphrase}));
      break;
    case syncer::KeyDerivationMethod::SCRYPT_8192_8_11:
      EXPECT_TRUE(base::Base64Decode(
          nigori.custom_passphrase_key_derivation_salt(), &decoded_salt));
      EXPECT_TRUE(cryptographer->DecryptPendingKeys(
          {syncer::KeyDerivationParams::CreateForScrypt(decoded_salt),
           passphrase}));
      break;
    case syncer::KeyDerivationMethod::UNSUPPORTED:
      // This test cannot pass since we wouldn't know how to decrypt data
      // encrypted using an unsupported method.
      ADD_FAILURE() << "Unsupported key derivation method encountered: "
                    << nigori.custom_passphrase_key_derivation_method();
  }

  return cryptographer;
}

sync_pb::NigoriSpecifics CreateCustomPassphraseNigori(
    const syncer::KeyParams& params) {
  syncer::KeyDerivationMethod method = params.derivation_params.method();

  sync_pb::NigoriSpecifics nigori;
  nigori.set_keybag_is_frozen(true);
  nigori.set_keystore_migration_time(1U);
  nigori.set_encrypt_everything(true);
  nigori.set_passphrase_type(sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE);
  nigori.set_custom_passphrase_key_derivation_method(
      EnumKeyDerivationMethodToProto(method));

  std::string encoded_salt;
  switch (method) {
    case syncer::KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
      // Nothing to do; no further information needs to be extracted from
      // Nigori.
      break;
    case syncer::KeyDerivationMethod::SCRYPT_8192_8_11:
      base::Base64Encode(params.derivation_params.scrypt_salt(), &encoded_salt);
      nigori.set_custom_passphrase_key_derivation_salt(encoded_salt);
      break;
    case syncer::KeyDerivationMethod::UNSUPPORTED:
      ADD_FAILURE()
          << "Unsupported method in KeyParams, cannot construct Nigori.";
      break;
  }

  // Nigori also contains a keybag, which is an encrypted collection of all keys
  // that the data might be encrypted with. To create it, we construct a
  // cryptographer, add our key to it, and use GetKeys() to dump it to the
  // keybag (in encrypted form). So, in our case, the keybag is simply the
  // passphrase-derived key encrypted with itself.  Note that this is usually
  // also the case during normal Sync operation, and so the keybag from Nigori
  // only helps the encryption machinery to know if a given key is correct (e.g.
  // checking if a user's passphrase is correct is done by trying to decrypt the
  // keybag using a key derived from that passphrase). However, in some migrated
  // states, the keybag might also additionally contain an old, pre-migration
  // key.
  syncer::DirectoryCryptographer cryptographer;
  bool add_key_result = cryptographer.AddKey(params);
  DCHECK(add_key_result);
  bool get_keys_result =
      cryptographer.GetKeys(nigori.mutable_encryption_keybag());
  DCHECK(get_keys_result);

  return nigori;
}

sync_pb::EntitySpecifics GetEncryptedBookmarkEntitySpecifics(
    const sync_pb::BookmarkSpecifics& bookmark_specifics,
    const syncer::KeyParams& key_params) {
  sync_pb::EntitySpecifics new_specifics;

  sync_pb::EntitySpecifics wrapped_entity_specifics;
  *wrapped_entity_specifics.mutable_bookmark() = bookmark_specifics;
  syncer::DirectoryCryptographer cryptographer;
  bool add_key_result = cryptographer.AddKey(key_params);
  DCHECK(add_key_result);
  bool encrypt_result = cryptographer.Encrypt(
      wrapped_entity_specifics, new_specifics.mutable_encrypted());
  DCHECK(encrypt_result);

  new_specifics.mutable_bookmark()->set_title("encrypted");
  new_specifics.mutable_bookmark()->set_url("encrypted");

  return new_specifics;
}

void SetNigoriInFakeServer(fake_server::FakeServer* fake_server,
                           const sync_pb::NigoriSpecifics& nigori) {
  std::string nigori_entity_id =
      fake_server->GetTopLevelPermanentItemId(syncer::NIGORI);
  ASSERT_NE(nigori_entity_id, "");
  sync_pb::EntitySpecifics nigori_entity_specifics;
  *nigori_entity_specifics.mutable_nigori() = nigori;
  fake_server->ModifyEntitySpecifics(nigori_entity_id, nigori_entity_specifics);
}

}  // namespace encryption_helper

ServerNigoriChecker::ServerNigoriChecker(
    syncer::ProfileSyncService* service,
    fake_server::FakeServer* fake_server,
    syncer::PassphraseType expected_passphrase_type)
    : SingleClientStatusChangeChecker(service),
      fake_server_(fake_server),
      expected_passphrase_type_(expected_passphrase_type) {}

bool ServerNigoriChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for a Nigori node with the proper passphrase type to become "
         "available on the server.";

  std::vector<sync_pb::SyncEntity> nigori_entities =
      fake_server_->GetPermanentSyncEntitiesByModelType(syncer::NIGORI);
  EXPECT_LE(nigori_entities.size(), 1U);
  return !nigori_entities.empty() &&
         syncer::ProtoPassphraseInt32ToEnum(
             nigori_entities[0].specifics().nigori().passphrase_type()) ==
             expected_passphrase_type_;
}

ServerNigoriKeyNameChecker::ServerNigoriKeyNameChecker(
    const std::string& expected_key_name,
    syncer::ProfileSyncService* service,
    fake_server::FakeServer* fake_server)
    : SingleClientStatusChangeChecker(service),
      fake_server_(fake_server),
      expected_key_name_(expected_key_name) {}

bool ServerNigoriKeyNameChecker::IsExitConditionSatisfied(std::ostream* os) {
  std::vector<sync_pb::SyncEntity> nigori_entities =
      fake_server_->GetPermanentSyncEntitiesByModelType(syncer::NIGORI);
  DCHECK_EQ(nigori_entities.size(), 1U);

  const std::string given_key_name =
      nigori_entities[0].specifics().nigori().encryption_keybag().key_name();

  *os << "Waiting for a Nigori node with proper key bag encryption key name ("
      << expected_key_name_ << ") to become available on the server."
      << "The server key bag encryption key name is " << given_key_name << ".";
  return given_key_name == expected_key_name_;
}

PassphraseRequiredStateChecker::PassphraseRequiredStateChecker(
    syncer::ProfileSyncService* service,
    bool desired_state)
    : SingleClientStatusChangeChecker(service), desired_state_(desired_state) {}

bool PassphraseRequiredStateChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting until decryption passphrase is " +
             std::string(desired_state_ ? "required" : "not required");
  return service()
             ->GetUserSettings()
             ->IsPassphraseRequiredForPreferredDataTypes() == desired_state_;
}

TrustedVaultKeyRequiredStateChecker::TrustedVaultKeyRequiredStateChecker(
    syncer::ProfileSyncService* service,
    bool desired_state)
    : SingleClientStatusChangeChecker(service), desired_state_(desired_state) {}

bool TrustedVaultKeyRequiredStateChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting until trusted vault keys are " +
             std::string(desired_state_ ? "required" : "not required");
  return service()
             ->GetUserSettings()
             ->IsTrustedVaultKeyRequiredForPreferredDataTypes() ==
         desired_state_;
}

ScopedScryptFeatureToggler::ScopedScryptFeatureToggler(
    bool force_disabled,
    bool use_for_new_passphrases) {
  std::vector<base::Feature> enabled_features;
  std::vector<base::Feature> disabled_features;
  if (force_disabled) {
    enabled_features.push_back(
        switches::kSyncForceDisableScryptForCustomPassphrase);
  } else {
    disabled_features.push_back(
        switches::kSyncForceDisableScryptForCustomPassphrase);
  }
  if (use_for_new_passphrases) {
    enabled_features.push_back(switches::kSyncUseScryptForNewCustomPassphrases);
  } else {
    disabled_features.push_back(
        switches::kSyncUseScryptForNewCustomPassphrases);
  }
  feature_list_.InitWithFeatures(enabled_features, disabled_features);
}
