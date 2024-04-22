// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/webauthn_credentials_helper.h"
#include "chrome/browser/ui/browser.h"
#include "components/sync/base/features.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using webauthn_credentials_helper::AwaitAllModelsMatch;
using webauthn_credentials_helper::GetModel;
using webauthn_credentials_helper::NewPasskey;

class TwoClientWebAuthnCredentialsSyncTest : public SyncTest {
 public:
  TwoClientWebAuthnCredentialsSyncTest() : SyncTest(TWO_CLIENT) {}
  ~TwoClientWebAuthnCredentialsSyncTest() override = default;

  base::test::ScopedFeatureList scoped_feature_list_{
      syncer::kSyncWebauthnCredentials};
};

IN_PROC_BROWSER_TEST_F(TwoClientWebAuthnCredentialsSyncTest,
                       E2E_ENABLED(AddAndDelete)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  webauthn::PasskeyModel& model0 = GetModel(0);
  EXPECT_EQ(model0.GetAllSyncIds().size(), 0u);
  sync_pb::WebauthnCredentialSpecifics passkey0 = NewPasskey();
  const std::string sync_id0 = model0.AddNewPasskeyForTesting(passkey0);
  EXPECT_EQ(model0.GetAllSyncIds().size(), 1u);

  webauthn::PasskeyModel& model1 = GetModel(1);
  ASSERT_TRUE(AwaitAllModelsMatch());
  EXPECT_EQ(model1.GetAllSyncIds().size(), 1u);

  const std::string sync_id1 = model1.AddNewPasskeyForTesting(NewPasskey());
  ASSERT_TRUE(AwaitAllModelsMatch());
  EXPECT_EQ(model1.GetAllSyncIds().size(), 2u);

  ASSERT_TRUE(model1.DeletePasskey(passkey0.credential_id(), FROM_HERE));
  ASSERT_TRUE(AwaitAllModelsMatch());
  EXPECT_EQ(model1.GetAllSyncIds().size(), 1u);
}

}  // namespace
