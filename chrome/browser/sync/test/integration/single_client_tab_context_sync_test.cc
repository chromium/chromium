// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/uuid.h"
#include "chrome/browser/sync/tab_context_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/model/crypto/agile_symmetric_key_set.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/protocol/agile_encryption_keys.pb.h"
#include "components/sync/protocol/encrypted_tab_context_container_specifics.pb.h"
#include "components/sync/protocol/encrypted_tab_context_item_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/fake_server.h"
#include "components/sync/test/fake_server_nigori_helper.h"
#include "components/sync/test/nigori_test_utils.h"
#include "components/sync_tab_context/container_id.h"
#include "components/sync_tab_context/fake_ephemeral_key_server.h"
#include "components/sync_tab_context/http_rpc_constants.h"
#include "components/sync_tab_context/tab_context_sync_service.h"
#include "components/sync_tab_context/upload_outcome.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "components/trusted_vault/trusted_vault_service.h"
#include "content/public/test/browser_test.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::Eq;
using ::testing::SizeIs;

std::vector<uint8_t> GetTestEncryptionKey() {
  return {1, 2, 3, 4};
}

void InjectEncryptedContainerToFakeServer(
    const sync_tab_context::ContainerId& container_id,
    const sync_pb::AgileSymmetricKeySet& key_set_proto,
    fake_server::FakeServer* fake_server) {
  CHECK(fake_server);
  sync_pb::EntitySpecifics unencrypted_specifics;
  sync_pb::EncryptedTabContextContainerSpecifics* const container_specifics =
      unencrypted_specifics.mutable_encrypted_tab_context_container();
  container_specifics->set_uuid(container_id.value().AsLowercaseString());
  *container_specifics->mutable_encryption_key() = key_set_proto;

  const syncer::KeyParamsForTesting key_params =
      syncer::TrustedVaultKeyParamsForTesting(GetTestEncryptionKey());
  std::unique_ptr<syncer::CryptographerImpl> cryptographer =
      syncer::CryptographerImpl::FromSingleKeyForTesting(
          key_params.password, key_params.derivation_params);

  sync_pb::EntitySpecifics encrypted_specifics;
  CHECK(cryptographer->Encrypt(unencrypted_specifics,
                               encrypted_specifics.mutable_encrypted()));
  encrypted_specifics.mutable_encrypted_tab_context_container();

  fake_server->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/container_id.value().AsLowercaseString(),
          /*client_tag=*/container_id.value().AsLowercaseString(),
          encrypted_specifics,
          /*creation_time=*/0,
          /*last_modified_time=*/0));
}

class SingleClientTabContextSyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SingleClientTabContextSyncTest() : SyncTest(SINGLE_CLIENT) {
    std::vector<base::test::FeatureRef> enabled_features = {
        syncer::kSyncEncryptedTabContextContainer};
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      enabled_features.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
    }
    feature_overrides_.InitWithFeatures(enabled_features, {});
  }

  SingleClientTabContextSyncTest(const SingleClientTabContextSyncTest&) =
      delete;
  SingleClientTabContextSyncTest& operator=(
      const SingleClientTabContextSyncTest&) = delete;

  ~SingleClientTabContextSyncTest() override = default;

  // SyncTest:
  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SyncTest::SetUpCommandLine(command_line);

    ephemeral_key_server_ =
        std::make_unique<sync_tab_context::FakeEphemeralKeyServer>(
            embedded_test_server()->base_url());
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &sync_tab_context::FakeEphemeralKeyServer::HandleRequest,
        base::Unretained(ephemeral_key_server_.get())));
    command_line->AppendSwitchASCII(
        sync_tab_context::kEphemeralKeyServerUrlSwitch,
        sync_tab_context::FakeEphemeralKeyServer::GetServerURL(
            embedded_test_server()->base_url())
            .spec());
  }

  void TearDown() override {
    // Shut down `embedded_test_server()` before `ephemeral_key_server_` is
    // destroyed.
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    SyncTest::TearDown();
  }

 protected:
  [[nodiscard]] bool SetupSyncWithTrustedVault() {
    fake_server::SetNigoriInFakeServer(
        syncer::BuildTrustedVaultNigoriSpecifics({GetTestEncryptionKey()}),
        GetFakeServer());
    if (!SetupClients()) {
      return false;
    }
    TrustedVaultServiceFactory::GetForProfile(GetProfile(0))
        ->GetTrustedVaultClient(trusted_vault::SecurityDomainId::kChromeSync)
        ->StoreKeys(
            GetClient(0)->GetGaiaIdForAccount(SyncTestAccount::kDefaultAccount),
            {GetTestEncryptionKey()},
            /*last_key_version=*/1,
            /*trigger=*/std::nullopt);
    return SetupSync();
  }

  sync_tab_context::TabContextSyncService& GetTabContextSyncService() const {
    sync_tab_context::TabContextSyncService* const service =
        TabContextSyncServiceFactory::GetForProfile(GetProfile(0));
    CHECK(service);
    return *service;
  }

  sync_tab_context::FakeEphemeralKeyServer* GetEphemeralKeyServer() {
    return ephemeral_key_server_.get();
  }

  sync_tab_context::UploadOutcome UploadPageContextAndWaitForCompletion(
      const sync_tab_context::ContainerId& container_id,
      const std::string& entry_id,
      std::string page_context) {
    base::test::TestFuture<sync_tab_context::UploadOutcome> future;
    GetTabContextSyncService().UploadPageContext(
        container_id, entry_id, std::move(page_context), future.GetCallback());
    return future.Get();
  }

  std::optional<std::string> GetContainerAccessToken(
      const sync_tab_context::ContainerId& container_id) {
    base::test::TestFuture<std::optional<std::string>> future;
    GetTabContextSyncService().GetContainerAccessToken(container_id,
                                                       future.GetCallback());
    return future.Take();
  }

 private:
  base::test::ScopedFeatureList feature_overrides_;
  std::unique_ptr<sync_tab_context::FakeEphemeralKeyServer>
      ephemeral_key_server_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SingleClientTabContextSyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(SingleClientTabContextSyncTest,
                       ShouldNotActivateWithoutTrustedVaultPassphrase) {
  ASSERT_TRUE(SetupSync());
  ASSERT_NE(GetSyncService(0)->GetUserSettings()->GetPassphraseType(),
            syncer::PassphraseType::kTrustedVaultPassphrase);

  EXPECT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::ENCRYPTED_TAB_CONTEXT_CONTAINER));
  EXPECT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::ENCRYPTED_TAB_CONTEXT_ITEM));
  EXPECT_EQ(GetTabContextSyncService().CreateContainer(), std::nullopt);
}

IN_PROC_BROWSER_TEST_P(SingleClientTabContextSyncTest,
                       ShouldCreateContainerUploadPageContextAndDecrypt) {
  ASSERT_TRUE(SetupSyncWithTrustedVault());
  ASSERT_THAT(GetSyncService(0)->GetUserSettings()->GetPassphraseType(),
              Eq(syncer::PassphraseType::kTrustedVaultPassphrase));
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::ENCRYPTED_TAB_CONTEXT_CONTAINER));
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::ENCRYPTED_TAB_CONTEXT_ITEM));

  const std::optional<sync_tab_context::ContainerId> container_id =
      GetTabContextSyncService().CreateContainer();
  ASSERT_TRUE(container_id.has_value());

  const std::string kEntryId = "entry_1";
  const std::string kPageContext =
      "Page context payload to compress, encrypt, commit, and decrypt.";
  ASSERT_EQ(UploadPageContextAndWaitForCompletion(*container_id, kEntryId,
                                                  kPageContext),
            sync_tab_context::UploadOutcome::kSucceeded);

  // Because `ENCRYPTED_TAB_CONTEXT_CONTAINER` has higher priority than
  // `ENCRYPTED_TAB_CONTEXT_ITEM`, `UploadPageContext()` completing guarantees
  // that the container has also been committed to the server.
  EXPECT_THAT(GetFakeServer()->GetSyncEntitiesByDataType(
                  syncer::ENCRYPTED_TAB_CONTEXT_CONTAINER),
              SizeIs(1));

  const std::vector<sync_pb::SyncEntity> server_item_entities =
      GetFakeServer()->GetSyncEntitiesByDataType(
          syncer::ENCRYPTED_TAB_CONTEXT_ITEM);
  ASSERT_THAT(server_item_entities, SizeIs(1));
  const sync_pb::EncryptedTabContextItemSpecifics& server_item_specifics =
      server_item_entities[0].specifics().encrypted_tab_context_item();
  EXPECT_EQ(server_item_specifics.container_id(),
            container_id->value().AsLowercaseString());
  EXPECT_EQ(server_item_specifics.item_id(), kEntryId);
  ASSERT_TRUE(server_item_specifics.has_encrypted_content());

  const std::optional<std::string> access_token =
      GetContainerAccessToken(*container_id);
  ASSERT_TRUE(access_token.has_value());

  EXPECT_EQ(GetEphemeralKeyServer()->DecryptContent(
                *access_token, server_item_specifics.encrypted_content()),
            kPageContext);
}

IN_PROC_BROWSER_TEST_P(SingleClientTabContextSyncTest,
                       ShouldDownloadRemoteContainerAndUploadPageContext) {
  const sync_tab_context::ContainerId remote_container_id(
      base::Uuid::GenerateRandomV4());
  std::unique_ptr<syncer::AgileSymmetricKeySet> remote_key_set =
      syncer::AgileSymmetricKeySet::CreateEmpty();
  remote_key_set->RotatePrimaryToNewlyGeneratedRandomKey();
  InjectEncryptedContainerToFakeServer(
      remote_container_id, remote_key_set->ToProto(), GetFakeServer());

  ASSERT_TRUE(SetupSyncWithTrustedVault());

  const std::string kEntryId = "entry_remote";
  const std::string kPageContext = "Page context for remote container.";
  ASSERT_EQ(UploadPageContextAndWaitForCompletion(remote_container_id, kEntryId,
                                                  kPageContext),
            sync_tab_context::UploadOutcome::kSucceeded);

  const std::vector<sync_pb::SyncEntity> server_item_entities =
      GetFakeServer()->GetSyncEntitiesByDataType(
          syncer::ENCRYPTED_TAB_CONTEXT_ITEM);
  ASSERT_THAT(server_item_entities, SizeIs(1));
  const sync_pb::EncryptedTabContextItemSpecifics& server_item_specifics =
      server_item_entities[0].specifics().encrypted_tab_context_item();
  EXPECT_EQ(server_item_specifics.container_id(),
            remote_container_id.value().AsLowercaseString());
  EXPECT_EQ(server_item_specifics.item_id(), kEntryId);

  const std::optional<std::string> access_token =
      GetContainerAccessToken(remote_container_id);
  ASSERT_TRUE(access_token.has_value());

  EXPECT_EQ(GetEphemeralKeyServer()->DecryptContent(
                *access_token, server_item_specifics.encrypted_content()),
            kPageContext);
}

IN_PROC_BROWSER_TEST_P(SingleClientTabContextSyncTest,
                       ShouldReturnNulloptWhenEphemeralKeyServerFails) {
  ASSERT_TRUE(SetupSyncWithTrustedVault());

  const std::optional<sync_tab_context::ContainerId> container_id =
      GetTabContextSyncService().CreateContainer();
  ASSERT_TRUE(container_id.has_value());

  GetEphemeralKeyServer()->SetHttpErrorToReturn(
      net::HTTP_INTERNAL_SERVER_ERROR);
  EXPECT_EQ(GetContainerAccessToken(*container_id), std::nullopt);
}

}  // namespace
