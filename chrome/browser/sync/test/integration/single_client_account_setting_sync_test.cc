// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/account_setting_service_factory.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/autofill/core/browser/webdata/account_settings/account_setting_service.h"
#include "components/autofill/core/browser/webdata/account_settings/account_setting_sync_util.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/protocol/account_setting_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/test/fake_server.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr std::string_view kWalletPrivacyContextualSurfacingSetting =
    "WALLET_PRIVACY_CONTEXTUAL_SURFACING";

using autofill::AccountSettingService;
using autofill::CreateSettingSpecifics;

// Waits until
// `AccountSettingService::IsWalletPrivacyContextualSurfacingEnabled()` has the
// `expected_state`.
// The condition is checked whenever sync's status changes - in particular, each
// time a sync cycle completes. This works, since setting changes are exposed
// through AccountSettingService synchronously after the operation on the bridge
// completes.
class WalletSurfacingChecker : public SingleClientStatusChangeChecker {
 public:
  WalletSurfacingChecker(syncer::SyncServiceImpl* sync_service,
                         AccountSettingService* setting_service,
                         bool expected_state)
      : SingleClientStatusChangeChecker(sync_service),
        setting_service_(setting_service),
        expected_state_(expected_state) {}

  // SingleClientStatusChangeChecker:
  bool IsExitConditionSatisfied(std::ostream* os) override {
    return setting_service_->IsWalletPrivacyContextualSurfacingEnabled() ==
           expected_state_;
  }

 private:
  const raw_ptr<AccountSettingService> setting_service_;
  const bool expected_state_;
};

// ACCOUNT_SETTING is supposed to behave the same in and outside of transport
// mode. These tests are parameterized by whether the test should run in
// transport mode (true) or not (false).
class SingleClientAccountSettingSyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SingleClientAccountSettingSyncTest() : SyncTest(SINGLE_CLIENT) {
    std::vector<base::test::FeatureRef> enabled_features = {
        syncer::kSyncAccountSettings};
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      enabled_features.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
    }
    feature_list_.InitWithFeatures(enabled_features, /*disabled_features=*/{});
  }

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

  AccountSettingService* GetAccountSettingService() {
    return autofill::AccountSettingServiceFactory::GetForBrowserContext(
        GetProfile(0));
  }

  void InjectSpecificsToServer(
      const sync_pb::AccountSettingSpecifics& specifics) {
    sync_pb::EntitySpecifics entity_specifics;
    *entity_specifics.mutable_account_setting() = specifics;
    GetFakeServer()->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/"account-setting",
            /*client_tag=*/
            specifics.name(), entity_specifics,
            /*creation_time=*/0, /*last_modified_time=*/0));
  }

  bool WaitForWalletSurfacingState(bool expected_state) {
    return WalletSurfacingChecker(GetClient(0)->service(),
                                  GetAccountSettingService(), expected_state)
        .Wait();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SingleClientAccountSettingSyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(SingleClientAccountSettingSyncTest, InitialSync) {
  InjectSpecificsToServer(
      CreateSettingSpecifics(kWalletPrivacyContextualSurfacingSetting, true));
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(WaitForWalletSurfacingState(true));
}

IN_PROC_BROWSER_TEST_P(SingleClientAccountSettingSyncTest,
                       IncrementalUpdate_Add) {
  ASSERT_TRUE(SetupSync());
  InjectSpecificsToServer(
      CreateSettingSpecifics(kWalletPrivacyContextualSurfacingSetting, true));
  EXPECT_TRUE(WaitForWalletSurfacingState(true));
}

IN_PROC_BROWSER_TEST_P(SingleClientAccountSettingSyncTest,
                       IncrementalUpdate_Update) {
  InjectSpecificsToServer(
      CreateSettingSpecifics(kWalletPrivacyContextualSurfacingSetting, true));
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(WaitForWalletSurfacingState(true));
  InjectSpecificsToServer(
      CreateSettingSpecifics(kWalletPrivacyContextualSurfacingSetting, false));
  EXPECT_TRUE(WaitForWalletSurfacingState(false));
}

IN_PROC_BROWSER_TEST_P(SingleClientAccountSettingSyncTest,
                       IncrementalUpdate_Remove) {
  InjectSpecificsToServer(
      CreateSettingSpecifics(kWalletPrivacyContextualSurfacingSetting, true));
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(WaitForWalletSurfacingState(true));
  // Simulate removing the `kIsEnabledSettingName` setting on the server.
  const std::string client_tag_hash =
      syncer::ClientTagHash::FromUnhashed(
          syncer::ACCOUNT_SETTING, kWalletPrivacyContextualSurfacingSetting)
          .value();
  GetFakeServer()->InjectEntity(
      syncer::PersistentTombstoneEntity::PersistentTombstoneEntity::CreateNew(
          syncer::LoopbackServerEntity::CreateId(syncer::ACCOUNT_SETTING,
                                                 client_tag_hash),
          client_tag_hash));
  // Non-existing settings behave as if they have their default value.
  EXPECT_TRUE(WaitForWalletSurfacingState(false));
}

// ChromeOS does not support signing out of the primary account.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(SingleClientAccountSettingSyncTest,
                       Signout_DataCleared) {
  InjectSpecificsToServer(
      CreateSettingSpecifics(kWalletPrivacyContextualSurfacingSetting, true));
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(WaitForWalletSurfacingState(true));
  GetClient(0)->SignOutPrimaryAccount();
  EXPECT_TRUE(WaitForWalletSurfacingState(false));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace
