// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"
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
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/loopback_server/loopback_server_entity.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using testing::IsEmpty;
using testing::UnorderedElementsAre;

using webauthn_credentials_helper::EntityHasDisplayName;
using webauthn_credentials_helper::EntityHasSyncId;
using webauthn_credentials_helper::EntityHasUsername;
using webauthn_credentials_helper::LocalPasskeysChangedChecker;
using webauthn_credentials_helper::LocalPasskeysMatchChecker;
using webauthn_credentials_helper::MockPasskeyModelObserver;
using webauthn_credentials_helper::NewPasskey;
using webauthn_credentials_helper::PasskeyHasSyncId;
using webauthn_credentials_helper::PasskeySyncActiveChecker;
using webauthn_credentials_helper::ServerPasskeysMatchChecker;

constexpr int kSingleProfile = 0;
constexpr char kUsername1[] = "anya";
constexpr char kDisplayName1[] = "Anya Forger";
constexpr char kUsername2[] = "yor";
constexpr char kDisplayName2[] = "Yor Forger";

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
      /*last_modified_time=*/0);
}

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

  webauthn::PasskeyModel& GetModel() {
    return webauthn_credentials_helper::GetModel(kSingleProfile);
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
}

// Adding a remote passkey should sync to the client.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       DownloadNewServerPasskey) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const std::string sync_id = InjectPasskeyToFakeServer(NewPasskey());
  EXPECT_TRUE(
      LocalPasskeysMatchChecker(kSingleProfile,
                                UnorderedElementsAre(PasskeyHasSyncId(sync_id)))
          .Wait());
}

// Getting passkeys by RP ID.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       GetPasskeysByRpId) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  constexpr char kRpId2[] = "rpid2.com";
  sync_pb::WebauthnCredentialSpecifics passkey1 = NewPasskey();
  sync_pb::WebauthnCredentialSpecifics passkey1_shadow = NewPasskey();
  passkey1.add_newly_shadowed_credential_ids(passkey1_shadow.credential_id());
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
  EXPECT_THAT(GetModel().GetPasskeysForRelyingPartyId(passkey2.rp_id()),
              UnorderedElementsAre(PasskeyHasSyncId(sync_id2)));
}

// Deleting a local passkey should remove from the server.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       UploadLocalPasskeyDeletion) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  sync_pb::WebauthnCredentialSpecifics passkey = NewPasskey();
  const std::string sync_id = GetModel().AddNewPasskeyForTesting(passkey);
  ASSERT_TRUE(
      ServerPasskeysMatchChecker(UnorderedElementsAre(EntityHasSyncId(sync_id)))
          .Wait());

  LocalPasskeysChangedChecker change_checker(kSingleProfile);
  GetModel().DeletePasskey(passkey.credential_id());
  EXPECT_TRUE(ServerPasskeysMatchChecker(IsEmpty()).Wait());
  EXPECT_TRUE(change_checker.Wait());
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
  EXPECT_TRUE(LocalPasskeysMatchChecker(kSingleProfile, IsEmpty()).Wait());
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
  EXPECT_FALSE(GetModel().DeletePasskey("non existing id"));
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
  sync_pb::WebauthnCredentialSpecifics passkey3 = NewPasskey();
  passkey3.add_newly_shadowed_credential_ids(passkey1.credential_id());
  passkey3.add_newly_shadowed_credential_ids(passkey2.credential_id());
  sync_pb::WebauthnCredentialSpecifics passkey4 = NewPasskey();
  passkey4.add_newly_shadowed_credential_ids(passkey3.credential_id());
  sync_pb::WebauthnCredentialSpecifics passkey5 = NewPasskey();
  passkey5.add_newly_shadowed_credential_ids(passkey4.credential_id());
  sync_pb::WebauthnCredentialSpecifics passkey6 = NewPasskey();

  GetModel().AddNewPasskeyForTesting(passkey1);
  GetModel().AddNewPasskeyForTesting(passkey2);
  GetModel().AddNewPasskeyForTesting(passkey3);
  GetModel().AddNewPasskeyForTesting(passkey4);
  GetModel().AddNewPasskeyForTesting(passkey5);
  GetModel().AddNewPasskeyForTesting(passkey6);

  ASSERT_TRUE(
      ServerPasskeysMatchChecker(testing::BeginEndDistanceIs(6)).Wait());

  // Delete passkey 4. This should result in only passkey 4 being deleted.
  ASSERT_TRUE(GetModel().DeletePasskey(passkey4.credential_id()));
  EXPECT_TRUE(ServerPasskeysMatchChecker(
                  UnorderedElementsAre(EntityHasSyncId(passkey1.sync_id()),
                                       EntityHasSyncId(passkey2.sync_id()),
                                       EntityHasSyncId(passkey3.sync_id()),
                                       EntityHasSyncId(passkey5.sync_id()),
                                       EntityHasSyncId(passkey6.sync_id())))
                  .Wait());

  // Delete passkey 5. This should also result in only passkey 5 being deleted.
  ASSERT_TRUE(GetModel().DeletePasskey(passkey5.credential_id()));
  EXPECT_TRUE(ServerPasskeysMatchChecker(
                  UnorderedElementsAre(EntityHasSyncId(passkey1.sync_id()),
                                       EntityHasSyncId(passkey2.sync_id()),
                                       EntityHasSyncId(passkey3.sync_id()),
                                       EntityHasSyncId(passkey6.sync_id())))
                  .Wait());

  // Delete passkey 6. All credentials should be deleted.
  ASSERT_TRUE(GetModel().DeletePasskey(passkey6.credential_id()));
  EXPECT_TRUE(ServerPasskeysMatchChecker(IsEmpty()).Wait());
}

// Deleting a passkey should not delete passkeys for a different rp id or user
// id.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       DoNotDeleteCredentialsForDifferentRpIdOrUserId) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  sync_pb::WebauthnCredentialSpecifics different_rp_id_passkey = NewPasskey();
  different_rp_id_passkey.set_rp_id("another-rpid.com");
  GetModel().AddNewPasskeyForTesting(different_rp_id_passkey);

  sync_pb::WebauthnCredentialSpecifics different_user_id_passkey = NewPasskey();
  different_user_id_passkey.set_user_id(base::RandBytesAsString(16));
  GetModel().AddNewPasskeyForTesting(different_user_id_passkey);

  sync_pb::WebauthnCredentialSpecifics passkey = NewPasskey();
  GetModel().AddNewPasskeyForTesting(passkey);

  ASSERT_EQ(passkey.rp_id(), different_user_id_passkey.rp_id());
  ASSERT_EQ(passkey.user_id(), different_rp_id_passkey.user_id());
  ASSERT_TRUE(ServerPasskeysMatchChecker(
                  UnorderedElementsAre(
                      EntityHasSyncId(passkey.sync_id()),
                      EntityHasSyncId(different_rp_id_passkey.sync_id()),
                      EntityHasSyncId(different_user_id_passkey.sync_id())))
                  .Wait());

  ASSERT_TRUE(GetModel().DeletePasskey(passkey.credential_id()));
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
  sync_pb::WebauthnCredentialSpecifics passkey2 = NewPasskey();
  passkey1.add_newly_shadowed_credential_ids(passkey2.credential_id());
  passkey2.add_newly_shadowed_credential_ids(passkey1.credential_id());
  GetModel().AddNewPasskeyForTesting(passkey1);
  GetModel().AddNewPasskeyForTesting(passkey2);
  EXPECT_TRUE(ServerPasskeysMatchChecker(
                  UnorderedElementsAre(EntityHasSyncId(passkey1.sync_id()),
                                       EntityHasSyncId(passkey2.sync_id())))
                  .Wait());

  ASSERT_FALSE(GetModel().DeletePasskey(passkey1.credential_id()));
  EXPECT_TRUE(ServerPasskeysMatchChecker(
                  UnorderedElementsAre(EntityHasSyncId(passkey1.sync_id()),
                                       EntityHasSyncId(passkey2.sync_id())))
                  .Wait());
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
  GetModel().DeletePasskey(passkey.credential_id());
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
  GetModel().AddNewPasskeyForTesting(passkey);
  EXPECT_TRUE(
      ServerPasskeysMatchChecker(UnorderedElementsAre(testing::AllOf(
                                     EntityHasUsername(kUsername1),
                                     EntityHasDisplayName(kDisplayName1))))
          .Wait());

  LocalPasskeysChangedChecker change_checker(kSingleProfile);
  EXPECT_TRUE(GetModel().UpdatePasskey(passkey.credential_id(),
                                       {
                                           .user_name = kUsername2,
                                           .user_display_name = kDisplayName2,
                                       }));
  EXPECT_TRUE(
      ServerPasskeysMatchChecker(UnorderedElementsAre(testing::AllOf(
                                     EntityHasUsername(kUsername2),
                                     EntityHasDisplayName(kDisplayName2))))
          .Wait());
  EXPECT_TRUE(change_checker.Wait());
  EXPECT_EQ(GetModel().GetAllPasskeys().at(0).user_name(), kUsername2);
  EXPECT_EQ(GetModel().GetAllPasskeys().at(0).user_display_name(),
            kDisplayName2);
}

// Tests that attempting to update a non existing passkey returns false.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       UpdateNonExistingPasskey) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  MockPasskeyModelObserver observer(&GetModel());
  EXPECT_CALL(observer, OnPasskeysChanged).Times(0);
  EXPECT_FALSE(GetModel().UpdatePasskey("non existing id",
                                        {
                                            .user_name = kUsername1,
                                            .user_display_name = kDisplayName1,
                                        }));
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
                                       }));
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       UpdatingPasskeysPersistsOverRestarts) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  ASSERT_EQ(GetModel().GetAllPasskeys().size(), 1u);
  EXPECT_EQ(GetModel().GetAllPasskeys().at(0).user_name(), kUsername1);
  EXPECT_EQ(GetModel().GetAllPasskeys().at(0).user_display_name(),
            kDisplayName1);
}

IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       LegacySyncIdCompatibility) {
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
// Tests that passkeys sync on transport mode only if the user has consented to
// showing credentials from their Google account.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       TransportModeConsent) {
  const std::string sync_id = InjectPasskeyToFakeServer(NewPasskey());
  ASSERT_TRUE(SetupClients());

  AccountInfo account_info = secondary_account_helper::SignInUnconsentedAccount(
      GetProfile(0), &test_url_loader_factory_, "user@email.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Passkeys should not be syncing.
  EXPECT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::WEBAUTHN_CREDENTIAL));

  // Let the user opt in to transport mode and wait for passkeys to start
  // syncing.
  password_manager::features_util::OptInToAccountStorage(
      GetProfile(0)->GetPrefs(), GetSyncService(0));
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
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
