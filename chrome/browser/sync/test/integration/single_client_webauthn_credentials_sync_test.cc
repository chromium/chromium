// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/webauthn_credentials_helper.h"
#include "chrome/browser/ui/browser.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/loopback_server/loopback_server_entity.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using webauthn_credentials_helper::GetModel;
using webauthn_credentials_helper::NewPasskey;

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

  base::test::ScopedFeatureList scoped_feature_list_{
      syncer::kSyncWebauthnCredentials};
};

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
  EXPECT_THAT(GetModel(0).GetAllSyncIds(),
              testing::UnorderedElementsAreArray(expected_sync_ids));
}

}  // namespace
