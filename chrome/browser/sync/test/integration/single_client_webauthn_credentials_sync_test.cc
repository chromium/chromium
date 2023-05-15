// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/webauthn_credentials_helper.h"
#include "chrome/browser/ui/browser.h"
#include "components/password_manager/core/browser/password_manager_features_util.h"
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

using testing::ElementsAre;
using testing::IsEmpty;

using webauthn_credentials_helper::EntityHasSyncId;
using webauthn_credentials_helper::LocalPasskeysMatchChecker;
using webauthn_credentials_helper::NewPasskey;
using webauthn_credentials_helper::PasskeyHasSyncId;
using webauthn_credentials_helper::PasskeySyncActiveChecker;
using webauthn_credentials_helper::ServerPasskeysMatchChecker;

constexpr int kSingleProfile = 0;

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

  PasskeyModel& GetModel() {
    return webauthn_credentials_helper::GetModel(kSingleProfile);
  }
};

// Adding a local passkey should sync to the server.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       UploadNewLocalPasskey) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const std::string sync_id = GetModel().AddNewPasskeyForTesting(NewPasskey());
  EXPECT_TRUE(
      ServerPasskeysMatchChecker(ElementsAre(EntityHasSyncId(sync_id))).Wait());
}

// Adding a remote passkey should sync to the client.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       DownloadNewServerPasskey) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const std::string sync_id = InjectPasskeyToFakeServer(NewPasskey());
  EXPECT_TRUE(LocalPasskeysMatchChecker(kSingleProfile,
                                        ElementsAre(PasskeyHasSyncId(sync_id)))
                  .Wait());
}

// Deleting a local passkey should remove from the server.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       UploadLocalPasskeyDeletion) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const std::string sync_id = GetModel().AddNewPasskeyForTesting(NewPasskey());
  ASSERT_TRUE(
      ServerPasskeysMatchChecker(ElementsAre(EntityHasSyncId(sync_id))).Wait());

  GetModel().DeletePasskeyForTesting(sync_id);
  EXPECT_TRUE(ServerPasskeysMatchChecker(IsEmpty()).Wait());
}

// Deleting a remote passkey should remove from the client.
IN_PROC_BROWSER_TEST_F(SingleClientWebAuthnCredentialsSyncTest,
                       DownloadServerPasskeyDeletion) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const std::string sync_id = GetModel().AddNewPasskeyForTesting(NewPasskey());
  ASSERT_TRUE(
      ServerPasskeysMatchChecker(ElementsAre(EntityHasSyncId(sync_id))).Wait());

  DeletePasskeyFromFakeServer(sync_id);
  EXPECT_TRUE(LocalPasskeysMatchChecker(kSingleProfile, IsEmpty()).Wait());
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
  EXPECT_TRUE(LocalPasskeysMatchChecker(kSingleProfile,
                                        ElementsAre(PasskeyHasSyncId(sync_id)))
                  .Wait());

  // Opt out. The passkey should be removed.
  password_manager::features_util::OptOutOfAccountStorageAndClearSettings(
      GetProfile(0)->GetPrefs(), GetSyncService(0));
  EXPECT_TRUE(LocalPasskeysMatchChecker(kSingleProfile, IsEmpty()).Wait());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
