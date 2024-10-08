// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/webauthn_credentials_helper.h"
#include "chrome/browser/ui/browser.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/loopback_server/loopback_server_entity.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/sync/test/test_matchers.h"
#include "components/version_info/version_info.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_change.h"
#include "components/webauthn/core/browser/passkey_model_utils.h"
#include "components/webauthn/core/browser/passkey_sync_bridge.h"
#include "content/public/test/browser_test.h"
#include "crypto/ec_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using testing::ElementsAre;
using testing::IsEmpty;
using testing::Optional;
using testing::UnorderedElementsAre;

using webauthn_credentials_helper::EntityHasDisplayName;
using webauthn_credentials_helper::EntityHasLastUsedTime;
using webauthn_credentials_helper::EntityHasSyncId;
using webauthn_credentials_helper::EntityHasUsername;
using webauthn_credentials_helper::kTestRpId;
using webauthn_credentials_helper::LocalPasskeysChangedChecker;
using webauthn_credentials_helper::LocalPasskeysMatchChecker;
using webauthn_credentials_helper::MockPasskeyModelObserver;
using webauthn_credentials_helper::NewPasskey;
using webauthn_credentials_helper::NewShadowingPasskey;
using webauthn_credentials_helper::PasskeyChangeObservationChecker;
using webauthn_credentials_helper::PasskeyHasDisplayName;
using webauthn_credentials_helper::PasskeyHasRpId;
using webauthn_credentials_helper::PasskeyHasSyncId;
using webauthn_credentials_helper::PasskeyHasUserId;
using webauthn_credentials_helper::PasskeySpecificsEq;
using webauthn_credentials_helper::PasskeySyncActiveChecker;
using webauthn_credentials_helper::ServerPasskeysMatchChecker;

constexpr int kSingleProfile = 0;
constexpr char kUsername1[] = "anya";
constexpr char kDisplayName1[] = "Anya Forger";
constexpr char kUsername2[] = "yor";
constexpr char kDisplayName2[] = "Yor Forger";
constexpr int64_t kLastUsedTime1 = 10;
constexpr int64_t kLastUsedTime2 = 20;

static const webauthn::PasskeyModel::UserEntity kTestUser(
    std::vector<uint8_t>{1, 2, 3},
    "user@example.com",
    "Example User");

constexpr std::array<uint8_t, 32> kTrustedVaultKey = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

constexpr int32_t kTrustedVaultKeyVersion = 23;

bool PublicKeyForPasskeyEquals(
    const sync_pb::WebauthnCredentialSpecifics& passkey,
    base::span<const uint8_t> trusted_vault_key,
    base::span<const uint8_t> expected_spki) {
  sync_pb::WebauthnCredentialSpecifics_Encrypted encrypted_data;
  CHECK(webauthn::passkey_model_utils::DecryptWebauthnCredentialSpecificsData(
      trusted_vault_key, passkey, &encrypted_data));
  auto ec_key = crypto::ECPrivateKey::CreateFromPrivateKeyInfo(
      base::as_bytes(base::make_span(encrypted_data.private_key())));
  CHECK(ec_key);
  std::vector<uint8_t> ec_key_pub;
  CHECK(ec_key->ExportPublicKey(&ec_key_pub));
  return base::ranges::equal(ec_key_pub, expected_spki);
}

std::unique_ptr<syncer::PersistentUniqueClientEntity>
CreateEntityWithCustomClientTagHash(
    const std::string& client_tag_hash,
    const sync_pb::WebauthnCredentialSpecifics& specifics) {
  sync_pb::EntitySpecifics entity;
  *entity.mutable_webauthn_credential() = specifics;
  return std::make_unique<syncer::PersistentUniqueClientEntity>(
      syncer::LoopbackServerEntity::CreateId(syncer::WEBAUTHN_CREDENTIAL,
                                             client_tag_hash),
      syncer::WEBAUTHN_CREDENTIAL, /*version=*/0,
      /*non_unique_name=*/"", client_tag_hash, entity, /*creation_time=*/0,
      /*last_modified_time=*/0, /*collaboration_id=*/"");
}

class PasskeyModelReadyChecker : public StatusChangeChecker,
                                 public webauthn::PasskeyModel::Observer {
 public:
  explicit PasskeyModelReadyChecker(webauthn::PasskeyModel* model)
      : model_(model) {
    observation_.Observe(model);
  }
  ~PasskeyModelReadyChecker() override = default;

  // SingleClientStatusChangeChecker:
  bool IsExitConditionSatisfied(std::ostream* os) override {
    return model_->IsReady();
  }

  // webauthn::PasskeyModel::Observer:
  void OnPasskeysChanged(
      const std::vector<webauthn::PasskeyModelChange>& changes) override {
    CheckExitCondition();
  }

  void OnPasskeyModelShuttingDown() override {}

  void OnPasskeyModelIsReady(bool is_ready) override {}

 private:
  const raw_ptr<webauthn::PasskeyModel> model_;
  base::ScopedObservation<webauthn::PasskeyModel,
                          webauthn::PasskeyModel::Observer>
      observation_{this};
};

class SingleClientWebAuthnCredentialsSyncTest : public SyncTest {
 public:
  SingleClientWebAuthnCredentialsSyncTest() : SyncTest(SINGLE_CLIENT) {}

  ~SingleClientWebAuthnCredentialsSyncTest() override = default;

  // Injects a new WEBAUTHN_CREDENTIAL type server entity and returns the
  // randomly generated `sync_id`.
  std::string InjectPasskeyToFakeServer(
      sync_pb::WebauthnCredentialSpecifics specifics) {
    const std::string sync_id = specifics.sync_id();
    sync_pb::EntitySpecifics entity;
    *entity.mutable_webauthn_credential() = std::move(specifics);
    fake_server_->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/"", /*client_tag=*/sync_id, entity,
            /*creation_time=*/0,
            /*last_modified_time=*/0));
    return sync_id;
  }

  // Marks the WEBAUTHN_CREDENTIAL with `sync_id` as deleted on the server.
  void DeletePasskeyFromFakeServer(const std::string& sync_id) {
    const std::string client_tag_hash =
        syncer::ClientTagHash::FromUnhashed(syncer::WEBAUTHN_CREDENTIAL,
                                            sync_id)
            .value();
    fake_server_->InjectEntity(
        syncer::PersistentTombstoneEntity::PersistentTombstoneEntity::CreateNew(
            syncer::LoopbackServerEntity::CreateId(syncer::WEBAUTHN_CREDENTIAL,
                                                   client_tag_hash),
            client_tag_hash));
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      syncer::kSyncWebauthnCredentials};

  webauthn::PasskeySyncBridge& GetModel() {
    return webauthn_credentials_helper::GetModel(kSingleProfile);
  }

  void WaitTillModelReady() {
    CHECK(PasskeyModelReadyChecker(&GetModel()).Wait());
  }
};

// Adding a local passkey should sync to the server.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       UploadNewLocalPasskey) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const std::string sync_id = GetModel().AddNewPasskeyForTesting(NewPasskey());
  EXPECT_TRUE(
      ServerPasskeysMatchChecker(UnorderedElementsAre(EntityHasSyncId(sync_id)))
          .Wait());

  // The client tag hash used should follow sync's usual rules. (Android's
  // implementation of passkeys uses a different client tag hash format.)
  std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByDataType(syncer::WEBAUTHN_CREDENTIAL);
  EXPECT_EQ(entities.size(), 1u);
  EXPECT_EQ(entities.front().specifics().webauthn_credential().sync_id(),
            sync_id);
  EXPECT_EQ(entities.front().client_tag_hash(),
            syncer::ClientTagHash::FromUnhashed(
                syncer::WEBAUTHN_CREDENTIAL,
                entities.front().specifics().webauthn_credential().sync_id())
                .value());
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       StartWithLocalPasskey) {
  // Exercise the case where PasskeySyncBridge::MergeFullSyncData has local
  // credentials that the server doesn't know about.
  ASSERT_TRUE(SetupClients());
  WaitTillModelReady();
  const std::string sync_id = GetModel().AddNewPasskeyForTesting(NewPasskey());
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  EXPECT_TRUE(
      ServerPasskeysMatchChecker(UnorderedElementsAre(EntityHasSyncId(sync_id)))
          .Wait());
}

// CreatePasskey should create a new passkey entity and upload it to the server.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest, CreatePasskey) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  std::vector<uint8_t> public_key_spki_der;
  const sync_pb::WebauthnCredentialSpecifics passkey =
      GetModel().CreatePasskey(kTestRpId, kTestUser, kTrustedVaultKey,
                               kTrustedVaultKeyVersion, &public_key_spki_der);

  EXPECT_TRUE(ServerPasskeysMatchChecker(
                  UnorderedElementsAre(EntityHasSyncId(passkey.sync_id())))
                  .Wait());

  EXPECT_THAT(GetModel().GetAllPasskeys().at(0), PasskeySpecificsEq(passkey));

  EXPECT_THAT(passkey, PasskeyHasRpId(kTestRpId));
  const std::string expected_user_id(
      reinterpret_cast<const char*>(kTestUser.id.data()), kTestUser.id.size());
  EXPECT_THAT(passkey, PasskeyHasUserId(expected_user_id));
  EXPECT_TRUE(PublicKeyForPasskeyEquals(passkey, kTrustedVaultKey,
                                        public_key_spki_der));
}

// Creating a new passkey should shadow passkeys for the same (RP ID, user ID).
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       CreatePasskeyWithShadows) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  sync_pb::WebauthnCredentialSpecifics passkey1a = NewPasskey();
  sync_pb::WebauthnCredentialSpecifics passkey1b = NewPasskey();
  // Make 1a shadow 1b implicitly, i.e. without setting
  // newly_shadowed_credential_ids.
  ASSERT_EQ(passkey1a.rp_id(), passkey1b.rp_id());
  passkey1b.set_user_id(passkey1a.user_id());
  // Add another that shadows 1b explicitly.
  sync_pb::WebauthnCredentialSpecifics passkey1c =
      NewShadowingPasskey(passkey1b);
  // These shouldn't shadow anything.
  sync_pb::WebauthnCredentialSpecifics passkey2 = NewPasskey();
  passkey2.set_rp_id("rpid2.com");
  passkey2.set_user_id(passkey1a.user_id());
  sync_pb::WebauthnCredentialSpecifics passkey3 = NewPasskey();

  GetModel().AddNewPasskeyForTesting(passkey1a);
  GetModel().AddNewPasskeyForTesting(passkey1b);
  GetModel().AddNewPasskeyForTesting(passkey1c);
  GetModel().AddNewPasskeyForTesting(passkey2);
  GetModel().AddNewPasskeyForTesting(passkey3);

  // Invoking CreatePasskey() for the given RP ID should shadow 1a, 1c and 1c,
  // but not 2 or 3 (different RP or user ID).
  auto user_id = base::as_byte_span(passkey1a.user_id());
  sync_pb::WebauthnCredentialSpecifics new_passkey =
      GetModel().CreatePasskey(passkey1a.rp_id(),
                               webauthn::PasskeyModel::UserEntity(
                                   {user_id.begin(), user_id.end()}, "", ""),
                               kTrustedVaultKey, kTrustedVaultKeyVersion,
                               /*public_key_spki_der_out=*/nullptr);

  EXPECT_THAT(
      new_passkey.newly_shadowed_credential_ids(),
      UnorderedElementsAre(passkey1a.credential_id(), passkey1b.credential_id(),
                           passkey1c.credential_id()));
  EXPECT_THAT(GetModel().GetPasskeysForRelyingPartyId(passkey1a.rp_id()),
              UnorderedElementsAre(PasskeyHasSyncId(passkey3.sync_id()),
                                   PasskeyHasSyncId(new_passkey.sync_id())));
  EXPECT_THAT(GetModel().GetPasskeysForRelyingPartyId(passkey2.rp_id()),
              UnorderedElementsAre(PasskeyHasSyncId(passkey2.sync_id())));
}

// Tests CreatePasskey from a pre-constructed WebAuthnCredentialSpecifics.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       CreatePasskeyFromEntity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  sync_pb::WebauthnCredentialSpecifics passkey = NewPasskey();
  GetModel().CreatePasskey(passkey);

  EXPECT_TRUE(ServerPasskeysMatchChecker(
                  UnorderedElementsAre(EntityHasSyncId(passkey.sync_id())))
                  .Wait());
  EXPECT_THAT(GetModel().GetAllPasskeys().at(0), PasskeySpecificsEq(passkey));
  EXPECT_THAT(passkey, PasskeyHasRpId(kTestRpId));
}

// Tests CreatePasskey from a pre-constructed WebAuthnCredentialSpecifics with
// shadow passkeys being added.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       CreatePasskeyFromEntityWithShadows) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  sync_pb::WebauthnCredentialSpecifics passkey1a = NewPasskey();
  sync_pb::WebauthnCredentialSpecifics passkey1b = NewPasskey();
  // Make 1a shadow 1b implicitly, i.e. without setting
  // newly_shadowed_credential_ids.
  passkey1b.set_user_id(passkey1a.user_id());
  // Add another that shadows 1b explicitly.
  sync_pb::WebauthnCredentialSpecifics passkey1c =
      NewShadowingPasskey(passkey1b);
  // These shouldn't shadow anything.
  sync_pb::WebauthnCredentialSpecifics passkey2 = NewPasskey();
  passkey2.set_rp_id("rpid2.com");
  passkey2.set_user_id(passkey1a.user_id());
  sync_pb::WebauthnCredentialSpecifics passkey3 = NewPasskey();

  GetModel().AddNewPasskeyForTesting(passkey1a);
  GetModel().AddNewPasskeyForTesting(passkey1b);
  GetModel().AddNewPasskeyForTesting(passkey1c);
  GetModel().AddNewPasskeyForTesting(passkey2);
  GetModel().AddNewPasskeyForTesting(passkey3);

  // Invoking CreatePasskey() for the given RP ID should shadow 1a, 1b and 1c,
  // but not 2 or 3 (different RP or user ID).
  sync_pb::WebauthnCredentialSpecifics new_passkey = NewPasskey();
  new_passkey.set_user_id(passkey1a.user_id());
  GetModel().CreatePasskey(new_passkey);

  EXPECT_THAT(
      new_passkey.newly_shadowed_credential_ids(),
      UnorderedElementsAre(passkey1a.credential_id(), passkey1b.credential_id(),
                           passkey1c.credential_id()));
  EXPECT_THAT(GetModel().GetPasskeysForRelyingPartyId(passkey1a.rp_id()),
              UnorderedElementsAre(PasskeyHasSyncId(passkey3.sync_id()),
                                   PasskeyHasSyncId(new_passkey.sync_id())));
  EXPECT_THAT(GetModel().GetPasskeysForRelyingPartyId(passkey2.rp_id()),
              UnorderedElementsAre(PasskeyHasSyncId(passkey2.sync_id())));
}

// Adding a remote passkey should sync to the client.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       DownloadNewServerPasskey) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const std::string sync_id = InjectPasskeyToFakeServer(NewPasskey());
  EXPECT_TRUE(PasskeyChangeObservationChecker(
                  kSingleProfile,
                  {{webauthn::PasskeyModelChange::ChangeType::ADD, sync_id}})
                  .Wait());
}

// The model should retrieve individual passkeys by RP ID and credential ID.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest, GetPasskeys) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  sync_pb::WebauthnCredentialSpecifics passkey1a = NewPasskey();
  sync_pb::WebauthnCredentialSpecifics passkey1b = NewPasskey();
  ASSERT_EQ(passkey1a.rp_id(), passkey1b.rp_id());

  constexpr char kRpId2[] = "rpid2.com";
  sync_pb::WebauthnCredentialSpecifics passkey2 = NewPasskey();
  passkey2.set_rp_id(kRpId2);

  const std::string sync_id1a = InjectPasskeyToFakeServer(passkey1a);
  const std::string sync_id1b = InjectPasskeyToFakeServer(passkey1b);
  const std::string sync_id2 = InjectPasskeyToFakeServer(passkey2);

  EXPECT_TRUE(
      LocalPasskeysMatchChecker(
          kSingleProfile, UnorderedElementsAre(PasskeyHasSyncId(sync_id1a),
                                               PasskeyHasSyncId(sync_id1b),
                                               PasskeyHasSyncId(sync_id2)))
          .Wait());

  EXPECT_THAT(GetModel().GetPasskeysForRelyingPartyId(passkey1a.rp_id()),
              UnorderedElementsAre(PasskeyHasSyncId(sync_id1a),
                                   PasskeyHasSyncId(sync_id1b)));
  EXPECT_THAT(GetModel().GetPasskeyByCredentialId(passkey1a.rp_id(),
                                                  passkey1a.credential_id()),
              Optional(PasskeyHasSyncId(sync_id1a)));
  EXPECT_THAT(GetModel().GetPasskeyByCredentialId(passkey1b.rp_id(),
                                                  passkey1b.credential_id()),
              Optional(PasskeyHasSyncId(sync_id1b)));
  EXPECT_EQ(
      GetModel().GetPasskeyByCredentialId(kRpId2, passkey1a.credential_id()),
      std::nullopt);

  EXPECT_THAT(GetModel().GetPasskeysForRelyingPartyId(kRpId2),
              UnorderedElementsAre(PasskeyHasSyncId(sync_id2)));
}

// When getting passkeys, shadowed entities should be ignored.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       GetPasskeysWithShadows) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  constexpr char kRpId2[] = "rpid2.com";
  sync_pb::WebauthnCredentialSpecifics passkey1_shadow = NewPasskey();
  sync_pb::WebauthnCredentialSpecifics passkey1 =
      NewShadowingPasskey(passkey1_shadow);
  sync_pb::WebauthnCredentialSpecifics passkey2 = NewPasskey();
  passkey2.set_rp_id(kRpId2);
  const std::string sync_id1 = InjectPasskeyToFakeServer(passkey1);
  const std::string sync_id1_shadow =
      InjectPasskeyToFakeServer(passkey1_shadow);
  const std::string sync_id2 = InjectPasskeyToFakeServer(passkey2);
  EXPECT_TRUE(LocalPasskeysMatchChecker(
                  kSingleProfile,
                  UnorderedElementsAre(PasskeyHasSyncId(sync_id1),
                                       PasskeyHasSyncId(sync_id1_shadow),
                                       PasskeyHasSyncId(sync_id2)))
                  .Wait());

  EXPECT_THAT(GetModel().GetPasskeysForRelyingPartyId(passkey1.rp_id()),
              UnorderedElementsAre(PasskeyHasSyncId(sync_id1)));
  EXPECT_THAT(GetModel().GetPasskeyByCredentialId(passkey1.rp_id(),
                                                  passkey1.credential_id()),
              Optional(PasskeyHasSyncId(sync_id1)));
  EXPECT_EQ(GetModel().GetPasskeyByCredentialId(
                passkey1_shadow.rp_id(), passkey1_shadow.credential_id()),
            std::nullopt);
  EXPECT_THAT(GetModel().GetPasskeysForRelyingPartyId(passkey2.rp_id()),
              UnorderedElementsAre(PasskeyHasSyncId(sync_id2)));
  EXPECT_THAT(
      GetModel().GetPasskeyByCredentialId(kRpId2, passkey2.credential_id()),
      Optional(PasskeyHasSyncId(sync_id2)));
}

// Deleting a local passkey should remove from the server.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       UploadLocalPasskeyDeletion) {
  const base::Location kLocation = FROM_HERE;

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  sync_pb::WebauthnCredentialSpecifics passkey = NewPasskey();
  const std::string sync_id = GetModel().AddNewPasskeyForTesting(passkey);
  ASSERT_TRUE(
      ServerPasskeysMatchChecker(UnorderedElementsAre(EntityHasSyncId(sync_id)))
          .Wait());

  PasskeyChangeObservationChecker change_checker(
      kSingleProfile,
      {{webauthn::PasskeyModelChange::ChangeType::REMOVE, sync_id}});
  GetModel().DeletePasskey(passkey.credential_id(), kLocation);
  EXPECT_TRUE(ServerPasskeysMatchChecker(IsEmpty()).Wait());
  EXPECT_TRUE(change_checker.Wait());

  EXPECT_THAT(GetFakeServer()->GetCommittedDeletionOrigins(
                  syncer::DataType::WEBAUTHN_CREDENTIAL),
              ElementsAre(syncer::MatchesDeletionOrigin(
                  version_info::GetVersionNumber(), kLocation)));
}

// Downloading a deletion for a passkey that does not exist locally should not
// crash.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       DownloadDeletionOfNonExistingLocalPasskey) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  auto metadata_change_list =
      std::make_unique<syncer::InMemoryMetadataChangeList>();
  syncer::EntityChangeList entity_changes;
  entity_changes.emplace_back(
      syncer::EntityChange::CreateDelete("unknown-sync-id"));
  ASSERT_NO_FATAL_FAILURE(GetModel().ApplyIncrementalSyncChanges(
      std::move(metadata_change_list), std::move(entity_changes)));
}

// Deleting a remote passkey should remove from the client.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       DownloadServerPasskeyDeletion) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const std::string sync_id = GetModel().AddNewPasskeyForTesting(NewPasskey());
  ASSERT_TRUE(
      ServerPasskeysMatchChecker(UnorderedElementsAre(EntityHasSyncId(sync_id)))
          .Wait());

  DeletePasskeyFromFakeServer(sync_id);
  PasskeyChangeObservationChecker change_checker(
      kSingleProfile,
      {{webauthn::PasskeyModelChange::ChangeType::REMOVE, sync_id}});
  EXPECT_TRUE(change_checker.Wait());
}

// Attempting to delete a passkey that does not exist should return false.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       DeleteNonExistingPasskey) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const std::string sync_id = GetModel().AddNewPasskeyForTesting(NewPasskey());
  ASSERT_TRUE(
      ServerPasskeysMatchChecker(UnorderedElementsAre(EntityHasSyncId(sync_id)))
          .Wait());

  MockPasskeyModelObserver observer(&GetModel());
  EXPECT_CALL(observer, OnPasskeysChanged).Times(0);
  EXPECT_FALSE(GetModel().DeletePasskey("non existing id", FROM_HERE));
  EXPECT_TRUE(
      ServerPasskeysMatchChecker(UnorderedElementsAre(EntityHasSyncId(sync_id)))
          .Wait());
}

// Deleting a passkey should also delete its shadowed credentials.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       DeleteShadowedPasskeys) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Set up the following chain:
  // id6 (latest)
  // id5 (older) --shadows--> id4 --shadows--> id3 +-shadows--> id2
  //                                               |
  //                                               +-shadows--> id1
  sync_pb::WebauthnCredentialSpecifics passkey1 = NewPasskey();
  sync_pb::WebauthnCredentialSpecifics passkey2 = NewPasskey();
  passkey2.set_user_id(passkey1.user_id());
  sync_pb::WebauthnCredentialSpecifics passkey3 = NewShadowingPasskey(passkey1);
  passkey3.add_newly_shadowed_credential_ids(passkey2.credential_id());
  sync_pb::WebauthnCredentialSpecifics passkey4 = NewShadowingPasskey(passkey3);
  sync_pb::WebauthnCredentialSpecifics passkey5 = NewShadowingPasskey(passkey4);
  sync_pb::WebauthnCredentialSpecifics passkey6 = NewShadowingPasskey(passkey5);
  // `passkey6` doesn't have explicit shadow entries, but shadows the others
  // implicitly because of same user_id and more recent timestamp.
  passkey6.clear_newly_shadowed_credential_ids();

  GetModel().AddNewPasskeyForTesting(passkey1);
  GetModel().AddNewPasskeyForTesting(passkey2);
  GetModel().AddNewPasskeyForTesting(passkey3);
  GetModel().AddNewPasskeyForTesting(passkey4);
  GetModel().AddNewPasskeyForTesting(passkey5);
  GetModel().AddNewPasskeyForTesting(passkey6);

  ASSERT_TRUE(
      ServerPasskeysMatchChecker(testing::BeginEndDistanceIs(6)).Wait());

  // Delete passkey 4. This should result in only passkey 4 being deleted.
  ASSERT_TRUE(GetModel().DeletePasskey(passkey4.credential_id(), FROM_HERE));
  EXPECT_TRUE(ServerPasskeysMatchChecker(
                  UnorderedElementsAre(EntityHasSyncId(passkey1.sync_id()),
                                       EntityHasSyncId(passkey2.sync_id()),
                                       EntityHasSyncId(passkey3.sync_id()),
                                       EntityHasSyncId(passkey5.sync_id()),
                                       EntityHasSyncId(passkey6.sync_id())))
                  .Wait());

  // Delete passkey 5. This should also result in only passkey 5 being deleted.
  ASSERT_TRUE(GetModel().DeletePasskey(passkey5.credential_id(), FROM_HERE));
  EXPECT_TRUE(ServerPasskeysMatchChecker(
                  UnorderedElementsAre(EntityHasSyncId(passkey1.sync_id()),
                                       EntityHasSyncId(passkey2.sync_id()),
                                       EntityHasSyncId(passkey3.sync_id()),
                                       EntityHasSyncId(passkey6.sync_id())))
                  .Wait());

  // Delete passkey 6. All credentials should be deleted.
  ASSERT_TRUE(GetModel().DeletePasskey(passkey6.credential_id(), FROM_HERE));
  EXPECT_TRUE(ServerPasskeysMatchChecker(IsEmpty()).Wait());
}

// Deleting a passkey should not delete passkeys for a different rp id or user
// id.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       DoNotDeleteCredentialsForDifferentRpIdOrUserId) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  sync_pb::WebauthnCredentialSpecifics passkey = NewPasskey();
  GetModel().AddNewPasskeyForTesting(passkey);

  sync_pb::WebauthnCredentialSpecifics different_rp_id_passkey = NewPasskey();
  different_rp_id_passkey.set_rp_id("another-rpid.com");
  different_rp_id_passkey.set_user_id(passkey.user_id());
  GetModel().AddNewPasskeyForTesting(different_rp_id_passkey);

  sync_pb::WebauthnCredentialSpecifics different_user_id_passkey = NewPasskey();
  ASSERT_EQ(different_user_id_passkey.rp_id(), passkey.rp_id());
  ASSERT_NE(different_user_id_passkey.user_id(), passkey.user_id());
  GetModel().AddNewPasskeyForTesting(different_user_id_passkey);

  ASSERT_TRUE(ServerPasskeysMatchChecker(
                  UnorderedElementsAre(
                      EntityHasSyncId(passkey.sync_id()),
                      EntityHasSyncId(different_rp_id_passkey.sync_id()),
                      EntityHasSyncId(different_user_id_passkey.sync_id())))
                  .Wait());

  ASSERT_TRUE(GetModel().DeletePasskey(passkey.credential_id(), FROM_HERE));
  EXPECT_TRUE(ServerPasskeysMatchChecker(
                  UnorderedElementsAre(
                      EntityHasSyncId(different_rp_id_passkey.sync_id()),
                      EntityHasSyncId(different_user_id_passkey.sync_id())))
                  .Wait());
}

// Attempt deleting a passkey that is part of a shadow chain circle.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       DeletePasskeyFromShadowChainCircle) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  sync_pb::WebauthnCredentialSpecifics passkey1 = NewPasskey();
  sync_pb::WebauthnCredentialSpecifics passkey2 = NewShadowingPasskey(passkey1);
  // Create a circular shadow chain.
  ASSERT_EQ(passkey2.newly_shadowed_credential_ids().at(0),
            passkey1.credential_id());
  passkey1.add_newly_shadowed_credential_ids(passkey2.credential_id());
  GetModel().AddNewPasskeyForTesting(passkey1);
  GetModel().AddNewPasskeyForTesting(passkey2);
  EXPECT_TRUE(ServerPasskeysMatchChecker(
                  UnorderedElementsAre(EntityHasSyncId(passkey1.sync_id()),
                                       EntityHasSyncId(passkey2.sync_id())))
                  .Wait());

  // Deleting should fail because neither passkey is head of a shadow chain.
  ASSERT_FALSE(GetModel().DeletePasskey(passkey1.credential_id(), FROM_HERE));
  ASSERT_FALSE(GetModel().DeletePasskey(passkey2.credential_id(), FROM_HERE));
  EXPECT_TRUE(ServerPasskeysMatchChecker(
                  UnorderedElementsAre(EntityHasSyncId(passkey1.sync_id()),
                                       EntityHasSyncId(passkey2.sync_id())))
                  .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       DeleteAllPasskeys) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  sync_pb::WebauthnCredentialSpecifics passkey1 = NewPasskey();
  sync_pb::WebauthnCredentialSpecifics passkey2 = NewPasskey();

  GetModel().AddNewPasskeyForTesting(passkey1);
  GetModel().AddNewPasskeyForTesting(passkey2);
  EXPECT_TRUE(ServerPasskeysMatchChecker(
                  UnorderedElementsAre(EntityHasSyncId(passkey1.sync_id()),
                                       EntityHasSyncId(passkey2.sync_id())))
                  .Wait());

  GetModel().DeleteAllPasskeys();
  EXPECT_TRUE(GetModel().GetAllPasskeys().empty());
  EXPECT_TRUE(ServerPasskeysMatchChecker(IsEmpty()).Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       DeleteAllPasskeysEmptyStore) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  EXPECT_TRUE(GetModel().GetAllPasskeys().empty());
  EXPECT_TRUE(ServerPasskeysMatchChecker(IsEmpty()).Wait());

  GetModel().DeleteAllPasskeys();

  EXPECT_TRUE(GetModel().GetAllPasskeys().empty());
  EXPECT_TRUE(ServerPasskeysMatchChecker(IsEmpty()).Wait());
}

// Tests that deleting a passkey is persisted across browser restarts.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       PRE_DeletingPasskeysPersistsOverRestarts) {
  ASSERT_TRUE(SetupSync());

  sync_pb::WebauthnCredentialSpecifics passkey = NewPasskey();
  GetModel().AddNewPasskeyForTesting(passkey);
  EXPECT_TRUE(ServerPasskeysMatchChecker(
                  UnorderedElementsAre(EntityHasSyncId(passkey.sync_id())))
                  .Wait());
  EXPECT_THAT(GetModel().GetAllPasskeys(),
              UnorderedElementsAre(PasskeyHasSyncId(passkey.sync_id())));
  GetModel().DeletePasskey(passkey.credential_id(), FROM_HERE);
  EXPECT_TRUE(GetModel().GetAllPasskeys().empty());
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       DeletingPasskeysPersistsOverRestarts) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  EXPECT_TRUE(GetModel().GetAllPasskeys().empty());
}

// Tests updating a passkey.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest, UpdatePasskey) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  sync_pb::WebauthnCredentialSpecifics passkey = NewPasskey();
  passkey.set_user_name(kUsername1);
  passkey.set_user_display_name(kDisplayName1);
  passkey.set_last_used_time_windows_epoch_micros(kLastUsedTime1);
  GetModel().AddNewPasskeyForTesting(passkey);
  EXPECT_TRUE(
      ServerPasskeysMatchChecker(UnorderedElementsAre(testing::AllOf(
                                     EntityHasUsername(kUsername1),
                                     EntityHasDisplayName(kDisplayName1),
                                     EntityHasLastUsedTime(kLastUsedTime1))))
          .Wait());

  PasskeyChangeObservationChecker change_checker(
      kSingleProfile,
      {{webauthn::PasskeyModelChange::ChangeType::UPDATE, passkey.sync_id()}});
  EXPECT_TRUE(GetModel().UpdatePasskey(passkey.credential_id(),
                                       {
                                           .user_name = kUsername2,
                                           .user_display_name = kDisplayName2,
                                       },
                                       /*updated_by_user=*/false));
  base::Time last_used_time2 = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(kLastUsedTime2));
  EXPECT_TRUE(GetModel().UpdatePasskeyTimestamp(passkey.credential_id(),
                                                last_used_time2));
  EXPECT_TRUE(
      ServerPasskeysMatchChecker(UnorderedElementsAre(testing::AllOf(
                                     EntityHasUsername(kUsername2),
                                     EntityHasDisplayName(kDisplayName2),
                                     EntityHasLastUsedTime(kLastUsedTime2))))
          .Wait());
  EXPECT_TRUE(change_checker.Wait());
  EXPECT_FALSE(GetModel().GetAllPasskeys().at(0).edited_by_user());
  EXPECT_EQ(GetModel().GetAllPasskeys().at(0).user_name(), kUsername2);
  EXPECT_EQ(GetModel().GetAllPasskeys().at(0).user_display_name(),
            kDisplayName2);
}

// Tests that attempting to update a non existing passkey returns false.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       UpdateNonExistingPasskey) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  sync_pb::WebauthnCredentialSpecifics passkey = NewPasskey();
  passkey.set_user_name(kUsername1);
  passkey.set_user_display_name(kDisplayName1);
  GetModel().AddNewPasskeyForTesting(passkey);
  EXPECT_TRUE(
      ServerPasskeysMatchChecker(UnorderedElementsAre(testing::AllOf(
                                     EntityHasUsername(kUsername1),
                                     EntityHasDisplayName(kDisplayName1))))
          .Wait());
  // Simulate the user explicitly requesting a passkey update.
  PasskeyChangeObservationChecker change_checker(
      kSingleProfile,
      {{webauthn::PasskeyModelChange::ChangeType::UPDATE, passkey.sync_id()}});
  EXPECT_TRUE(GetModel().UpdatePasskey(passkey.credential_id(),
                                       {
                                           .user_name = kUsername2,
                                           .user_display_name = kDisplayName2,
                                       },
                                       /*updated_by_user=*/true));
  EXPECT_TRUE(
      ServerPasskeysMatchChecker(UnorderedElementsAre(testing::AllOf(
                                     EntityHasUsername(kUsername2),
                                     EntityHasDisplayName(kDisplayName2))))
          .Wait());
  EXPECT_TRUE(change_checker.Wait());
  EXPECT_TRUE(GetModel().GetAllPasskeys().at(0).edited_by_user());
  EXPECT_EQ(GetModel().GetAllPasskeys().at(0).user_name(), kUsername2);
  EXPECT_EQ(GetModel().GetAllPasskeys().at(0).user_display_name(),
            kDisplayName2);

  // Simulate an update that was not requested by the user.
  EXPECT_FALSE(GetModel().UpdatePasskey(passkey.credential_id(),
                                        {
                                            .user_name = kUsername1,
                                            .user_display_name = kDisplayName1,
                                        },
                                        /*updated_by_user=*/false));
  // Make sure no changes were done.
  EXPECT_TRUE(
      ServerPasskeysMatchChecker(UnorderedElementsAre(testing::AllOf(
                                     EntityHasUsername(kUsername2),
                                     EntityHasDisplayName(kDisplayName2))))
          .Wait());
  EXPECT_TRUE(GetModel().GetAllPasskeys().at(0).edited_by_user());
  EXPECT_EQ(GetModel().GetAllPasskeys().at(0).user_name(), kUsername2);
  EXPECT_EQ(GetModel().GetAllPasskeys().at(0).user_display_name(),
            kDisplayName2);
}

// Tests that attempting to update a passkey that was previously edited by the
// user is rejected.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       UpdatePasskeyEditedByUser) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  MockPasskeyModelObserver observer(&GetModel());
  EXPECT_CALL(observer, OnPasskeysChanged).Times(0);
  EXPECT_FALSE(GetModel().UpdatePasskey("non existing id",
                                        {
                                            .user_name = kUsername1,
                                            .user_display_name = kDisplayName1,
                                        },
                                        /*updated_by_user=*/false));
}

// Tests that updating a passkey is persisted across browser restarts.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       PRE_UpdatingPasskeysPersistsOverRestarts) {
  ASSERT_TRUE(SetupSync());

  sync_pb::WebauthnCredentialSpecifics passkey = NewPasskey();
  GetModel().AddNewPasskeyForTesting(passkey);
  EXPECT_TRUE(ServerPasskeysMatchChecker(
                  UnorderedElementsAre(EntityHasSyncId(passkey.sync_id())))
                  .Wait());
  EXPECT_THAT(GetModel().GetAllPasskeys(),
              UnorderedElementsAre(PasskeyHasSyncId(passkey.sync_id())));
  EXPECT_TRUE(GetModel().UpdatePasskey(passkey.credential_id(),
                                       {
                                           .user_name = kUsername1,
                                           .user_display_name = kDisplayName1,
                                       },
                                       /*updated_by_user=*/false));
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       UpdatingPasskeysPersistsOverRestarts) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  ASSERT_EQ(GetModel().GetAllPasskeys().size(), 1u);
  EXPECT_FALSE(GetModel().GetAllPasskeys().at(0).edited_by_user());
  EXPECT_EQ(GetModel().GetAllPasskeys().at(0).user_name(), kUsername1);
  EXPECT_EQ(GetModel().GetAllPasskeys().at(0).user_display_name(),
            kDisplayName1);
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       LegacySyncIdCompatibilityUponInitialDownload) {
  // Ordinarily, client_tag_hash is derived from the 16-byte `sync_id`.
  // Internally, it's computed as Base64(SHA1(prefix + client_tag)), which is 28
  // bytes long.
  std::vector<std::string> expected_sync_ids;
  {
    sync_pb::WebauthnCredentialSpecifics specifics1 = NewPasskey();
    expected_sync_ids.push_back(specifics1.sync_id());
    std::string client_tag_hash1 =
        syncer::ClientTagHash::FromUnhashed(syncer::WEBAUTHN_CREDENTIAL,
                                            specifics1.sync_id())
            .value();
    fake_server_->InjectEntity(
        CreateEntityWithCustomClientTagHash(client_tag_hash1, specifics1));
  }

  // But older Play Services clients set the `client_tag_hash` to be the
  // hex-encoded sync_id`.
  {
    sync_pb::WebauthnCredentialSpecifics specifics2 = NewPasskey();
    expected_sync_ids.push_back(specifics2.sync_id());
    fake_server_->InjectEntity(CreateEntityWithCustomClientTagHash(
        /*client_tag_hash=*/base::HexEncode(
            base::as_bytes(base::make_span(specifics2.sync_id()))),
        specifics2));
  }

  // Test upper and lower case hex encoding (in practice, Play Services uses
  // lower case).
  {
    sync_pb::WebauthnCredentialSpecifics specifics3 = NewPasskey();
    expected_sync_ids.push_back(specifics3.sync_id());
    fake_server_->InjectEntity(CreateEntityWithCustomClientTagHash(
        /*client_tag_hash=*/base::ToLowerASCII(base::HexEncode(
            base::as_bytes(base::make_span(specifics3.sync_id())))),
        specifics3));
  }

  // Also test some invalid client tag hash values are ignored:
  // Client tag hash has an entirely different format.
  {
    sync_pb::WebauthnCredentialSpecifics specifics4 = NewPasskey();
    fake_server_->InjectEntity(CreateEntityWithCustomClientTagHash(
        /*client_tag_hash=*/"INVALID", specifics4));
  }

  // Client tag hash is 16 byte hex, but encoding an unrelated sync ID.
  {
    sync_pb::WebauthnCredentialSpecifics specifics5 = NewPasskey();
    sync_pb::WebauthnCredentialSpecifics specifics6 = NewPasskey();
    fake_server_->InjectEntity(CreateEntityWithCustomClientTagHash(
        /*client_tag_hash=*/base::HexEncode(
            base::as_bytes(base::make_span(specifics6.sync_id()))),
        specifics5));
  }

  // Ensure the expected styles of client_tag_hash sync, but none of the invalid
  // ones do.
  ASSERT_TRUE(SetupSync());
  EXPECT_THAT(GetModel().GetAllSyncIds(),
              testing::UnorderedElementsAreArray(expected_sync_ids));
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       LegacySyncIdCompatibilityUponIncrementalUpdate) {
  ASSERT_TRUE(SetupSync());

  // Ordinarily, client_tag_hash is derived from the 16-byte `sync_id`.
  // Internally, it's computed as Base64(SHA1(prefix + client_tag)), which is 28
  // bytes long.
  std::vector<std::string> expected_sync_ids;
  {
    sync_pb::WebauthnCredentialSpecifics specifics1 = NewPasskey();
    expected_sync_ids.push_back(specifics1.sync_id());
    std::string client_tag_hash1 =
        syncer::ClientTagHash::FromUnhashed(syncer::WEBAUTHN_CREDENTIAL,
                                            specifics1.sync_id())
            .value();
    fake_server_->InjectEntity(
        CreateEntityWithCustomClientTagHash(client_tag_hash1, specifics1));
  }

  // But older Play Services clients set the `client_tag_hash` to be the
  // hex-encoded sync_id`.
  {
    sync_pb::WebauthnCredentialSpecifics specifics2 = NewPasskey();
    expected_sync_ids.push_back(specifics2.sync_id());
    fake_server_->InjectEntity(CreateEntityWithCustomClientTagHash(
        /*client_tag_hash=*/base::HexEncode(
            base::as_bytes(base::make_span(specifics2.sync_id()))),
        specifics2));
  }

  // Test upper and lower case hex encoding (in practice, Play Services uses
  // lower case).
  {
    sync_pb::WebauthnCredentialSpecifics specifics3 = NewPasskey();
    expected_sync_ids.push_back(specifics3.sync_id());
    fake_server_->InjectEntity(CreateEntityWithCustomClientTagHash(
        /*client_tag_hash=*/base::ToLowerASCII(base::HexEncode(
            base::as_bytes(base::make_span(specifics3.sync_id())))),
        specifics3));
  }

  // Also test some invalid client tag hash values are ignored:
  // Client tag hash has an entirely different format.
  {
    sync_pb::WebauthnCredentialSpecifics specifics4 = NewPasskey();
    fake_server_->InjectEntity(CreateEntityWithCustomClientTagHash(
        /*client_tag_hash=*/"INVALID", specifics4));
  }

  // Client tag hash is 16 byte hex, but encoding an unrelated sync ID.
  {
    sync_pb::WebauthnCredentialSpecifics specifics5 = NewPasskey();
    sync_pb::WebauthnCredentialSpecifics specifics6 = NewPasskey();
    fake_server_->InjectEntity(CreateEntityWithCustomClientTagHash(
        /*client_tag_hash=*/base::HexEncode(
            base::as_bytes(base::make_span(specifics6.sync_id()))),
        specifics5));
  }

  // Add one dummy regular entity for the purpose of waiting it is downloaded.
  {
    sync_pb::WebauthnCredentialSpecifics barrier_passkey = NewPasskey();
    const std::string barrier_sync_id =
        InjectPasskeyToFakeServer(barrier_passkey);
    ASSERT_TRUE(LocalPasskeysMatchChecker(
                    kSingleProfile,
                    testing::Contains(PasskeyHasSyncId(barrier_sync_id)))
                    .Wait());
    expected_sync_ids.push_back(barrier_passkey.sync_id());
  }

  // Ensure the expected styles of client_tag_hash sync, but none of the invalid
  // ones do.
  EXPECT_THAT(GetModel().GetAllSyncIds(),
              testing::UnorderedElementsAreArray(expected_sync_ids));
}

// Updating a remote passkey should sync to the client.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       DownloadPasskeyUpdate) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  sync_pb::WebauthnCredentialSpecifics passkey = NewPasskey();
  const std::string sync_id = InjectPasskeyToFakeServer(passkey);
  EXPECT_TRUE(PasskeyChangeObservationChecker(
                  kSingleProfile,
                  {{webauthn::PasskeyModelChange::ChangeType::ADD, sync_id}})
                  .Wait());

  // Update the credential's display name.
  passkey.set_user_display_name(kDisplayName2);
  InjectPasskeyToFakeServer(passkey);
  EXPECT_TRUE(PasskeyChangeObservationChecker(
                  kSingleProfile,
                  {{webauthn::PasskeyModelChange::ChangeType::UPDATE, sync_id}})
                  .Wait());
}

// Tests that disabling sync before sync startup correctly clears the passkey
// cache.
// Regression test for crbug.com/1476895.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       PRE_ClearingModelDataOnSyncStartup) {
  ASSERT_TRUE(SetupSync());

  sync_pb::WebauthnCredentialSpecifics passkey = NewPasskey();
  GetModel().AddNewPasskeyForTesting(passkey);
  EXPECT_TRUE(ServerPasskeysMatchChecker(
                  UnorderedElementsAre(EntityHasSyncId(passkey.sync_id())))
                  .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       ClearingModelDataOnSyncStartup) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->DisableSyncForAllDatatypes());
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());

  ASSERT_EQ(GetModel().GetAllPasskeys().size(), 0u);
}

// The unconsented primary account isn't supported on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)

class SingleClientWebAuthnCredentialsSyncTestExplicitParamTest
    : public SingleClientWebAuthnCredentialsSyncTest,
      public testing::WithParamInterface<bool /*explicit_signin*/> {
 public:
  SingleClientWebAuthnCredentialsSyncTestExplicitParamTest() = default;

  bool is_explicit_signin() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kExplicitBrowserSigninUIOnDesktop};
};

// Tests that passkeys sync on transport mode only if the user has consented to
// showing credentials from their Google account.
IN_PROC_BROWSER_TEST_P(SingleClientWebAuthnCredentialsSyncTestExplicitParamTest,
                       TransportModeConsent) {
  const std::string sync_id = InjectPasskeyToFakeServer(NewPasskey());
  ASSERT_TRUE(SetupClients());

  const char kTestEmail[] = "user@email.com";
  AccountInfo account_info =
      is_explicit_signin()
          ? secondary_account_helper::SignInUnconsentedAccount(
                GetProfile(0), &test_url_loader_factory_, kTestEmail)
          : secondary_account_helper::ImplicitSignInUnconsentedAccount(
                GetProfile(0), &test_url_loader_factory_, kTestEmail);
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  if (!is_explicit_signin()) {
    // Passkeys should be syncing only if the signin is explicit.
    EXPECT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(
        syncer::WEBAUTHN_CREDENTIAL));

    // Let the user opt in to transport mode and wait for passkeys to start
    // syncing.
    password_manager::features_util::OptInToAccountStorage(
        GetProfile(0)->GetPrefs(), GetSyncService(0));
  }
  PasskeySyncActiveChecker(GetSyncService(0)).Wait();
  EXPECT_TRUE(
      LocalPasskeysMatchChecker(kSingleProfile,
                                UnorderedElementsAre(PasskeyHasSyncId(sync_id)))
          .Wait());

  // Opt out. The passkey should be removed.
  password_manager::features_util::OptOutOfAccountStorageAndClearSettings(
      GetProfile(0)->GetPrefs(), GetSyncService(0));
  EXPECT_TRUE(LocalPasskeysMatchChecker(kSingleProfile, IsEmpty()).Wait());
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    SingleClientWebAuthnCredentialsSyncTestExplicitParamTest,
    ::testing::Bool(),
    [](const testing::TestParamInfo<bool>& info) {
      return info.param ? "Explicit" : "Implicit";
    });
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
