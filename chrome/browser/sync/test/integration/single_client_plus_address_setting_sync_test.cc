// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/plus_addresses/plus_address_setting_service_factory.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/settings/plus_address_setting_service.h"
#include "components/plus_addresses/settings/plus_address_setting_sync_test_util.h"
#include "components/plus_addresses/settings/plus_address_setting_sync_util.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/plus_address_setting_specifics.pb.h"
#include "components/sync/test/fake_server.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kIsEnabledSettingName[] = "has_feature_enabled";

using plus_addresses::CreateSettingSpecifics;
using plus_addresses::HasBoolSetting;
using plus_addresses::PlusAddressSettingService;
using testing::Property;

// Waits until `PlusAddressSettingService::GetIsPlusAddressesEnabled()` has the
// `expected_state`.
// The condition is checked whenever sync's status changes - in particular, each
// time a sync cycle completes. This works, since setting changes are exposed
// through PlusAddressSettingService synchronously after the operation on the
// bridge completes.
class PlusAddressEnabledChecker : public SingleClientStatusChangeChecker {
 public:
  PlusAddressEnabledChecker(syncer::SyncServiceImpl* sync_service,
                            PlusAddressSettingService* setting_service,
                            bool expected_state)
      : SingleClientStatusChangeChecker(sync_service),
        setting_service_(setting_service),
        expected_state_(expected_state) {}

  // SingleClientStatusChangeChecker:
  bool IsExitConditionSatisfied(std::ostream* os) override {
    return setting_service_->GetIsPlusAddressesEnabled() == expected_state_;
  }

 private:
  const raw_ptr<PlusAddressSettingService> setting_service_;
  const bool expected_state_;
};

// Waits until the fake server's PlusAddressSettingSpecifics contain matching
// specifics.
class FakeServerSpecificsChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  explicit FakeServerSpecificsChecker(
      const testing::Matcher<sync_pb::PlusAddressSettingSpecifics> matcher)
      : matcher_(matcher) {}

  // SingleClientStatusChangeChecker:
  bool IsExitConditionSatisfied(std::ostream* os) override {
    testing::StringMatchResultListener listener;
    bool matches = testing::ExplainMatchResult(
        testing::Contains(
            Property(&sync_pb::SyncEntity::specifics,
                     Property(&sync_pb::EntitySpecifics::plus_address_setting,
                              matcher_))),
        fake_server()->GetSyncEntitiesByDataType(syncer::PLUS_ADDRESS_SETTING),
        &listener);
    *os << listener.str();
    return matches;
  }

 private:
  const testing::Matcher<sync_pb::PlusAddressSettingSpecifics> matcher_;
};

// PLUS_ADDRESS_SETTING is supposed to behave the same in and outside of
// transport mode. These tests are parameterized by whether the test should run
// in transport mode (true) or not (false).
class SingleClientPlusAddressSettingSyncTest
    : public SyncTest,
      public testing::WithParamInterface<bool> {
 public:
  SingleClientPlusAddressSettingSyncTest() : SyncTest(SINGLE_CLIENT) {
    features_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{plus_addresses::features::kPlusAddressesEnabled,
                               {{plus_addresses::features::
                                     kEnterprisePlusAddressServerUrl.name,
                                 "https://not-used.com"}}},
                              {syncer::kSyncPlusAddressSetting, {}}},
        /*disabled_features=*/{});
  }

  // Sets up the sync client in sync-the-feature or sync-the-transport mode,
  // depending on `GetParam()`. Returns true if setup succeeded.
  bool SetupSync() {
    const bool should_run_in_transport_mode = GetParam();
    if (should_run_in_transport_mode) {
      return SetupClients() && GetClient(0)->SignInPrimaryAccount() &&
             GetClient(0)->AwaitSyncTransportActive();
    }
    return SyncTest::SetupSync();
  }

  PlusAddressSettingService* GetPlusAddressSettingService() {
    return PlusAddressSettingServiceFactory::GetForBrowserContext(
        GetProfile(0));
  }

  void InjectSpecificsToServer(
      const sync_pb::PlusAddressSettingSpecifics& specifics) {
    sync_pb::EntitySpecifics entity_specifics;
    *entity_specifics.mutable_plus_address_setting() = specifics;
    GetFakeServer()->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/"plus-address-setting",
            /*client_tag=*/
            specifics.name(), entity_specifics,
            /*creation_time=*/0, /*last_modified_time=*/0));
  }

  bool WaitForPlusAddressEnabledState(bool expected_state) {
    return PlusAddressEnabledChecker(GetClient(0)->service(),
                                     GetPlusAddressSettingService(),
                                     expected_state)
        .Wait();
  }

 private:
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    SingleClientPlusAddressSettingSyncTest,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // On ChromeOS, sync-the-feature gets started automatically once a primary
    // account is signed in and transport mode is not a thing. As such, only run
    // the tests in sync-the-feature mode.
    testing::Values(false)
#else
    testing::Bool()
#endif
);

IN_PROC_BROWSER_TEST_P(SingleClientPlusAddressSettingSyncTest, InitialSync) {
  InjectSpecificsToServer(
      plus_addresses::CreateSettingSpecifics(kIsEnabledSettingName, true));
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(WaitForPlusAddressEnabledState(true));
}

IN_PROC_BROWSER_TEST_P(SingleClientPlusAddressSettingSyncTest,
                       IncrementalUpdate_Add) {
  ASSERT_TRUE(SetupSync());
  InjectSpecificsToServer(
      plus_addresses::CreateSettingSpecifics(kIsEnabledSettingName, true));
  EXPECT_TRUE(WaitForPlusAddressEnabledState(true));
}

IN_PROC_BROWSER_TEST_P(SingleClientPlusAddressSettingSyncTest,
                       IncrementalUpdate_Update) {
  InjectSpecificsToServer(
      plus_addresses::CreateSettingSpecifics(kIsEnabledSettingName, true));
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(WaitForPlusAddressEnabledState(true));
  InjectSpecificsToServer(
      plus_addresses::CreateSettingSpecifics(kIsEnabledSettingName, false));
  EXPECT_TRUE(WaitForPlusAddressEnabledState(false));
}

IN_PROC_BROWSER_TEST_P(SingleClientPlusAddressSettingSyncTest,
                       IncrementalUpdate_Remove) {
  InjectSpecificsToServer(
      plus_addresses::CreateSettingSpecifics(kIsEnabledSettingName, true));
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(WaitForPlusAddressEnabledState(true));
  // Simulate removing the `kIsEnabledSettingName` setting on the server.
  const std::string client_tag_hash =
      syncer::ClientTagHash::FromUnhashed(syncer::PLUS_ADDRESS_SETTING,
                                          kIsEnabledSettingName)
          .value();
  GetFakeServer()->InjectEntity(
      syncer::PersistentTombstoneEntity::PersistentTombstoneEntity::CreateNew(
          syncer::LoopbackServerEntity::CreateId(syncer::PLUS_ADDRESS_SETTING,
                                                 client_tag_hash),
          client_tag_hash));
  // Non-existing settings behave as if they have their (setting-specific)
  // default value - which is true for the enabled setting.
  EXPECT_TRUE(WaitForPlusAddressEnabledState(true));
}

IN_PROC_BROWSER_TEST_P(SingleClientPlusAddressSettingSyncTest,
                       WriteAcceptedNotice) {
  ASSERT_TRUE(SetupSync());
  ASSERT_FALSE(GetPlusAddressSettingService()->GetHasAcceptedNotice());
  GetPlusAddressSettingService()->SetHasAcceptedNotice();
  EXPECT_TRUE(GetPlusAddressSettingService()->GetHasAcceptedNotice());
  EXPECT_TRUE(
      FakeServerSpecificsChecker(HasBoolSetting("has_accepted_notice", true))
          .Wait());
}

// ChromeOS does not support signing out of the primary account.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(SingleClientPlusAddressSettingSyncTest,
                       Signout_DataCleared) {
  InjectSpecificsToServer(
      plus_addresses::CreateSettingSpecifics(kIsEnabledSettingName, false));
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(WaitForPlusAddressEnabledState(false));
  GetClient(0)->SignOutPrimaryAccount();
  // The enabled setting defaults to true if the state is unknown.
  EXPECT_TRUE(WaitForPlusAddressEnabledState(true));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
