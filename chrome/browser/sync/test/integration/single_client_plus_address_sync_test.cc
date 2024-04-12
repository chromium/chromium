// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/webdata/plus_address_sync_util.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/loopback_server/persistent_tombstone_entity.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/test/fake_server.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using plus_addresses::EntityDataFromPlusProfile;
using plus_addresses::PlusAddressService;
using plus_addresses::PlusProfile;
using plus_addresses::test::CreatePlusProfile;

// Blocks until `service->GetPlusProfiles()` matches `matcher`.
// Since `PlusAddressService` lives on a different sequence than the bridge,
// waiting is necessary even after `SetupSync()`.
class PlusProfileChecker : public StatusChangeChecker,
                           public PlusAddressService::Observer {
 public:
  PlusProfileChecker(PlusAddressService* service,
                     testing::Matcher<std::vector<PlusProfile>> matcher)
      : service_(service), matcher_(std::move(matcher)) {
    scoped_observation_.Observe(service_);
  }

  // StatusChangeChecker:
  bool IsExitConditionSatisfied(std::ostream* os) override {
    testing::StringMatchResultListener listener;
    bool matches = testing::ExplainMatchResult(
        matcher_, service_->GetPlusProfiles(), &listener);
    *os << listener.str();
    return matches;
  }

  // PlusAddressService::Observer:
  void OnPlusAddressesChanged() override { CheckExitCondition(); }

 private:
  const raw_ptr<PlusAddressService> service_;
  const testing::Matcher<std::vector<PlusProfile>> matcher_;
  base::ScopedObservation<PlusAddressService, PlusAddressService::Observer>
      scoped_observation_{this};
};

// PLUS_ADDRESS is supposed to behave the same in and outside of transport mode.
// These tests are parameterized by whether the test should run in transport
// mode (true) or not (false).
class SingleClientPlusAddressSyncTest
    : public SyncTest,
      public testing::WithParamInterface<bool> {
 public:
  SingleClientPlusAddressSyncTest() : SyncTest(SINGLE_CLIENT) {
    features_.InitWithFeatures(
        /*enabled_features=*/{plus_addresses::features::kPlusAddressesEnabled,
                              syncer::kSyncPlusAddress},
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

  PlusAddressService* GetPlusAddressService() {
    return PlusAddressServiceFactory::GetForBrowserContext(GetProfile(0));
  }

  // Expects that `specifics` has `PlusAddressSpecifics`.
  void InjectEntityToServer(const sync_pb::EntitySpecifics& specifics) {
    GetFakeServer()->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/"plus-profile",
            /*client_tag=*/
            specifics.plus_address().profile_id(), specifics,
            /*creation_time=*/0, /*last_modified_time=*/0));
  }

  // Injects a tombstone for the PLUS_ADDRESS entity with given `client_tag`.
  void InjectTombstoneToServer(const std::string& client_tag) {
    const std::string client_tag_hash =
        syncer::ClientTagHash::FromUnhashed(syncer::PLUS_ADDRESS, client_tag)
            .value();
    fake_server_->InjectEntity(
        syncer::PersistentTombstoneEntity::PersistentTombstoneEntity::CreateNew(
            syncer::LoopbackServerEntity::CreateId(syncer::PLUS_ADDRESS,
                                                   client_tag_hash),
            client_tag_hash));
  }

 private:
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    SingleClientPlusAddressSyncTest,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // On ChromeOS, sync-the-feature gets started automatically once a primary
    // account is signed in and transport mode is not a thing. As such, only run
    // the tests in sync-the-feature mode.
    testing::Values(false)
#else
    testing::Bool()
#endif
);

IN_PROC_BROWSER_TEST_P(SingleClientPlusAddressSyncTest, InitialSync) {
  // Start syncing with an existing `plus_profile` on the server.
  const PlusProfile plus_profile = CreatePlusProfile();
  InjectEntityToServer(EntityDataFromPlusProfile(plus_profile).specifics);
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(PlusProfileChecker(GetPlusAddressService(),
                                 testing::UnorderedElementsAre(plus_profile))
                  .Wait());
}

IN_PROC_BROWSER_TEST_P(SingleClientPlusAddressSyncTest, IncrementalUpdate_Add) {
  ASSERT_TRUE(SetupSync());
  // Simulate creating a new `plus_profile` on the server after sync started.
  const PlusProfile plus_profile = CreatePlusProfile();
  InjectEntityToServer(EntityDataFromPlusProfile(plus_profile).specifics);
  EXPECT_TRUE(PlusProfileChecker(GetPlusAddressService(),
                                 testing::UnorderedElementsAre(plus_profile))
                  .Wait());
}

IN_PROC_BROWSER_TEST_P(SingleClientPlusAddressSyncTest,
                       IncrementalUpdate_Update) {
  PlusProfile plus_profile = CreatePlusProfile();
  InjectEntityToServer(EntityDataFromPlusProfile(plus_profile).specifics);
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(PlusProfileChecker(GetPlusAddressService(),
                                 testing::UnorderedElementsAre(plus_profile))
                  .Wait());
  // Simulate updating the `plus_profile` on the server.
  plus_profile.plus_address = "new-" + plus_profile.plus_address;
  InjectEntityToServer(EntityDataFromPlusProfile(plus_profile).specifics);
  EXPECT_TRUE(PlusProfileChecker(GetPlusAddressService(),
                                 testing::UnorderedElementsAre(plus_profile))
                  .Wait());
}

IN_PROC_BROWSER_TEST_P(SingleClientPlusAddressSyncTest,
                       IncrementalUpdate_Remove) {
  const PlusProfile plus_profile = CreatePlusProfile();
  InjectEntityToServer(EntityDataFromPlusProfile(plus_profile).specifics);
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(PlusProfileChecker(GetPlusAddressService(),
                                 testing::UnorderedElementsAre(plus_profile))
                  .Wait());
  // Simulate removing the `plus_profile` on the server.
  InjectTombstoneToServer(plus_profile.profile_id);
  EXPECT_TRUE(
      PlusProfileChecker(GetPlusAddressService(), testing::IsEmpty()).Wait());
}

// ChromeOS does not support signing out of the primary account.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(SingleClientPlusAddressSyncTest, Signout_DataCleared) {
  InjectEntityToServer(
      EntityDataFromPlusProfile(CreatePlusProfile()).specifics);
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(PlusProfileChecker(GetPlusAddressService(),
                                 testing::Not(testing::IsEmpty()))
                  .Wait());
  GetClient(0)->SignOutPrimaryAccount();
  EXPECT_TRUE(
      PlusProfileChecker(GetPlusAddressService(), testing::IsEmpty()).Wait());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
