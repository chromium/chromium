// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <variant>
#include <vector>

#include "base/test/protobuf_matchers.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/autofill/valuables_data_manager_factory.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager_test_utils.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager_test_utils.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/browser/test_utils/valuables_data_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_sync_util.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace {

using autofill::AttributeTypeName;
using autofill::AutofillEntityDataManagerFactory;
using autofill::EntityDataChangedWaiter;
using autofill::EntityDataManager;
using autofill::EntityInstance;
using autofill::LoyaltyCard;
using autofill::ValuablesDataChangedWaiter;
using autofill::ValuablesDataManager;
using autofill::ValuablesDataManagerFactory;
using autofill::test::CreateLoyaltyCard;
using autofill::test::CreateLoyaltyCard2;
using base::test::EqualsProto;
using sync_datatype_helper::test;
using testing::Contains;
using testing::ElementsAre;
using testing::UnorderedElementsAre;

// AUTOFILL_VALUABLE is responsible for syncing AutofillAi EntityInstances (like
// vehicles, flights, passports, etc) and LoyaltyCards (a non-AutofillAi type).
using ValuableEntity = std::variant<LoyaltyCard, EntityInstance>;

// Several different EntityInstance types are synced through AUTOFILL_VALUABLE.
// For the purposes of this integration test, they all behave identically and
// the tests use vehicle for simplicity. The sync util tests verify the entity
// specific conversion logic.
EntityInstance GetServerVehicleEntityInstanceWithRandomGuid(
    autofill::test::VehicleOptions options = {}) {
  // Clear any attributes that are not expected to be received from Wallet.
  options.nickname = "";
  options.date_modified = {};
  options.use_date = {};
  options.record_type = EntityInstance::RecordType::kServerWallet;
  return autofill::test::GetVehicleEntityInstanceWithRandomGuid(options);
}

autofill::AttributeInstance MakeAttribute(AttributeTypeName type_name,
                                          std::u16string_view value) {
  autofill::AttributeInstance attribute((autofill::AttributeType(type_name)));
  attribute.SetInfo(
      attribute.type().field_type(), std::u16string(value), "en-US",
      /*format_string=*/std::nullopt, autofill::VerificationStatus::kNoStatus);
  return attribute;
}

sync_pb::SyncEntity LoyaltyCardToSyncEntity(const LoyaltyCard& loyalty_card) {
  sync_pb::SyncEntity entity;
  entity.set_name(std::string(loyalty_card.id()));
  entity.set_id_string(std::string(loyalty_card.id()));
  entity.set_version(0);  // Will be overridden by the fake server.
  entity.set_ctime(12345);
  entity.set_mtime(12345);
  sync_pb::AutofillValuableSpecifics* valuable_specifics =
      entity.mutable_specifics()->mutable_autofill_valuable();
  valuable_specifics->set_id(std::string(loyalty_card.id()));

  sync_pb::LoyaltyCard* loyalty_card_specifics =
      valuable_specifics->mutable_loyalty_card();
  loyalty_card_specifics->set_merchant_name(loyalty_card.merchant_name());
  loyalty_card_specifics->set_program_name(loyalty_card.program_name());
  loyalty_card_specifics->set_program_logo(loyalty_card.program_logo().spec());
  loyalty_card_specifics->set_loyalty_card_number(
      loyalty_card.loyalty_card_number());
  for (const GURL& url : loyalty_card.merchant_domains()) {
    *loyalty_card_specifics->add_merchant_domains() = url.spec();
  }
  return entity;
}

sync_pb::SyncEntity EntityInstanceToSyncEntity(
    const EntityInstance& entity_instance) {
  sync_pb::SyncEntity entity;
  entity.set_name(std::string(entity_instance.guid()));
  entity.set_id_string(std::string(entity_instance.guid()));
  entity.set_version(0);  // Will be overridden by the fake server.
  entity.set_ctime(12345);
  entity.set_mtime(12345);
  sync_pb::AutofillValuableSpecifics* valuable_specifics =
      entity.mutable_specifics()->mutable_autofill_valuable();
  *valuable_specifics =
      autofill::CreateSpecificsFromEntityInstance(entity_instance,
                                                  /*base_specifics=*/{});
  return entity;
}

// Since the sync server operates in terms of entity specifics, this helper
// function converts a given `entity_instance` to the equivalent
// `AutofillValuableSpecifics`.
sync_pb::AutofillValuableSpecifics AsAutofillValuableSpecifics(
    const EntityInstance& entity_instance) {
  return autofill::CreateSpecificsFromEntityInstance(entity_instance,
                                                     /*base_specifics=*/{});
}

// Helper class to wait until the fake server's AutofillValuableSpecifics match
// a given predicate.
class FakeServerSpecificsChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  using Matcher =
      testing::Matcher<std::vector<sync_pb::AutofillValuableSpecifics>>;

  explicit FakeServerSpecificsChecker(const Matcher& matcher)
      : matcher_(matcher) {}

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    std::vector<sync_pb::AutofillValuableSpecifics> specifics;
    for (const sync_pb::SyncEntity& entity :
         fake_server()->GetSyncEntitiesByDataType(syncer::AUTOFILL_VALUABLE)) {
      specifics.push_back(entity.specifics().autofill_valuable());
    }
    testing::StringMatchResultListener listener;
    bool matches = testing::ExplainMatchResult(matcher_, specifics, &listener);
    *os << listener.str();
    return matches;
  }

 private:
  const Matcher matcher_;
};

class SingleClientValuablesSyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SingleClientValuablesSyncTest() : SyncTest(SINGLE_CLIENT) {
    std::vector<base::test::FeatureRef> enabled_features = {
        syncer::kSyncWalletVehicleRegistrations};
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      enabled_features.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
    }
    feature_list_.InitWithFeatures(enabled_features, {});
  }
  SingleClientValuablesSyncTest(const SingleClientValuablesSyncTest&) = delete;
  SingleClientValuablesSyncTest& operator=(
      const SingleClientValuablesSyncTest&) = delete;

  ~SingleClientValuablesSyncTest() override = default;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

  ValuablesDataManager& GetValuablesDataManager() {
    return *ValuablesDataManagerFactory::GetForProfile(test()->GetProfile(0));
  }

  EntityDataManager& GetEntityDataManager() {
    return *AutofillEntityDataManagerFactory::GetForProfile(
        test()->GetProfile(0));
  }

  void InjectValuableEntityIncrementally(
      const ValuableEntity& valuable_entity) {
    sync_pb::SyncEntity sync_entity =
        std::visit(absl::Overload{[&](const LoyaltyCard& card) {
                                    return LoyaltyCardToSyncEntity(card);
                                  },
                                  [&](const EntityInstance& entity) {
                                    return EntityInstanceToSyncEntity(entity);
                                  }},
                   valuable_entity);
    GetFakeServer()->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/sync_entity.name(),
            /*client_tag=*/sync_entity.id_string(), sync_entity.specifics(),
            /*creation_time=*/sync_entity.ctime(),
            /*last_modified_time=*/sync_entity.mtime()));
  }

  void SetFakeServerValuables(const std::vector<ValuableEntity>& entities) {
    GetFakeServer()->DeleteAllEntitiesForDataType(syncer::AUTOFILL_VALUABLE);
    for (const ValuableEntity& entity : entities) {
      InjectValuableEntityIncrementally(entity);
    }
  }

  void EnterSyncPausedStateForPrimaryAccount() {
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      GetClient(0)->EnterSignInPendingStateForPrimaryAccount();
    } else {
      GetClient(0)->EnterSyncPausedStateForPrimaryAccount();
    }
  }

  void ExitSyncPausedStateForPrimaryAccount() {
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      GetClient(0)->ExitSignInPendingStateForPrimaryAccount();
    } else {
      GetClient(0)->ExitSyncPausedStateForPrimaryAccount();
    }
  }

  void WaitForNumberOfLoyaltyCards(size_t expected_count) {
    ValuablesDataManager& vdm = GetValuablesDataManager();
    while (vdm.GetLoyaltyCards().size() != expected_count ||
           vdm.HasPendingQueries()) {
      ValuablesDataChangedWaiter(&vdm).Wait();
    }
  }

  void WaitForNumberOfEntityInstances(size_t expected_count) {
    EntityDataManager& edm = GetEntityDataManager();
    while (edm.GetEntityInstances().size() != expected_count ||
           edm.HasPendingQueries()) {
      EntityDataChangedWaiter(&edm).Wait();
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Valuables should get loaded on initial sync.
IN_PROC_BROWSER_TEST_P(SingleClientValuablesSyncTest, InitialSync) {
  const LoyaltyCard loyalty_card = CreateLoyaltyCard();
  const EntityInstance vehicle = GetServerVehicleEntityInstanceWithRandomGuid();
  SetFakeServerValuables({loyalty_card, vehicle});
  ASSERT_TRUE(SetupSync());
  EXPECT_THAT(GetValuablesDataManager().GetLoyaltyCards(),
              ElementsAre(loyalty_card));
  EXPECT_THAT(GetEntityDataManager().GetEntityInstances(),
              UnorderedElementsAre(vehicle));
}

// ChromeOS does not support late signin after profile creation, so the test
// below does not apply, at least in the current form.
#if !BUILDFLAG(IS_CHROMEOS)
// Valuables should get cleared from the database when the user signs out.
IN_PROC_BROWSER_TEST_P(SingleClientValuablesSyncTest, ClearOnSignOut) {
  const LoyaltyCard loyalty_card = CreateLoyaltyCard();
  const EntityInstance vehicle = GetServerVehicleEntityInstanceWithRandomGuid();
  SetFakeServerValuables({loyalty_card, vehicle});
  ASSERT_TRUE(SetupSync());
  ASSERT_THAT(GetValuablesDataManager().GetLoyaltyCards(),
              ElementsAre(loyalty_card));
  ASSERT_THAT(GetEntityDataManager().GetEntityInstances(),
              UnorderedElementsAre(vehicle));

  // Signout, the data & metadata should be gone.
  GetClient(0)->SignOutPrimaryAccount();
  WaitForNumberOfLoyaltyCards(0);
  WaitForNumberOfEntityInstances(0);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Valuables should get cleared from the database when the user enters the sync
// paused state (e.g. persistent auth error).
IN_PROC_BROWSER_TEST_P(SingleClientValuablesSyncTest, ClearOnSyncPaused) {
  const LoyaltyCard loyalty_card = CreateLoyaltyCard();
  const EntityInstance vehicle = GetServerVehicleEntityInstanceWithRandomGuid();
  SetFakeServerValuables({loyalty_card, vehicle});
  ASSERT_TRUE(SetupSync());
  ASSERT_THAT(GetValuablesDataManager().GetLoyaltyCards(),
              ElementsAre(loyalty_card));
  ASSERT_THAT(GetEntityDataManager().GetEntityInstances(),
              UnorderedElementsAre(vehicle));

  // Enter sync paused state, the data & metadata should be gone.
  EnterSyncPausedStateForPrimaryAccount();
  WaitForNumberOfLoyaltyCards(0);
  WaitForNumberOfEntityInstances(0);

  // When exiting the sync paused state, the data should be redownloaded.
  ExitSyncPausedStateForPrimaryAccount();
  WaitForNumberOfLoyaltyCards(1);
  WaitForNumberOfEntityInstances(1);
}

// Valuables are not using incremental updates. Make sure existing data gets
// replaced when synced down.
IN_PROC_BROWSER_TEST_P(SingleClientValuablesSyncTest,
                       NewSyncDataShouldReplaceExistingData) {
  const LoyaltyCard loyalty_card1 = CreateLoyaltyCard();
  const EntityInstance vehicle1 =
      GetServerVehicleEntityInstanceWithRandomGuid();
  SetFakeServerValuables({loyalty_card1, vehicle1});
  ASSERT_TRUE(SetupSync());
  ValuablesDataManager& vdm = GetValuablesDataManager();
  ASSERT_THAT(vdm.GetLoyaltyCards(), ElementsAre(loyalty_card1));
  EntityDataManager& edm = GetEntityDataManager();
  ASSERT_THAT(edm.GetEntityInstances(), UnorderedElementsAre(vehicle1));

  // Put some completely new data in the sync server and trigger full update.
  const LoyaltyCard loyalty_card2 = CreateLoyaltyCard2();
  const EntityInstance vehicle2 =
      GetServerVehicleEntityInstanceWithRandomGuid();
  {
    ValuablesDataChangedWaiter vdm_waiter(&vdm);
    EntityDataChangedWaiter edm_waiter(&edm);
    SetFakeServerValuables({loyalty_card2, vehicle2});
    std::move(vdm_waiter).Wait();
    std::move(edm_waiter).Wait();
  }
  EXPECT_THAT(vdm.GetLoyaltyCards(), ElementsAre(loyalty_card2));
  EXPECT_THAT(edm.GetEntityInstances(), UnorderedElementsAre(vehicle2));
}

// Valuables should get cleared from the database when the user disables
// payments sync.
IN_PROC_BROWSER_TEST_P(SingleClientValuablesSyncTest,
                       ClearOnDisablePaymentsSync) {
  const LoyaltyCard loyalty_card = CreateLoyaltyCard();
  const EntityInstance vehicle = GetServerVehicleEntityInstanceWithRandomGuid();
  SetFakeServerValuables({loyalty_card, vehicle});
  ASSERT_TRUE(SetupSync());
  ASSERT_THAT(GetValuablesDataManager().GetLoyaltyCards(),
              ElementsAre(loyalty_card));
  ASSERT_THAT(GetEntityDataManager().GetEntityInstances(),
              UnorderedElementsAre(vehicle));

  // Turn off payments sync, the data & metadata should be gone.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kPayments));

  WaitForNumberOfLoyaltyCards(0);
  WaitForNumberOfEntityInstances(0);
}

// Valuables should get cleared from the database when the user disables wallet
// autofill.
IN_PROC_BROWSER_TEST_P(SingleClientValuablesSyncTest,
                       ClearOnDisableWalletAutofill) {
  const LoyaltyCard loyalty_card = CreateLoyaltyCard();
  const EntityInstance vehicle = GetServerVehicleEntityInstanceWithRandomGuid();
  SetFakeServerValuables({loyalty_card, vehicle});
  ASSERT_TRUE(SetupSync());
  ASSERT_THAT(GetValuablesDataManager().GetLoyaltyCards(),
              ElementsAre(loyalty_card));
  ASSERT_THAT(GetEntityDataManager().GetEntityInstances(),
              UnorderedElementsAre(vehicle));

  // Turn off the wallet autofill pref, the data & metadata should be gone as a
  // side effect of the wallet data type controller noticing.
  GetSyncService(0)->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, /*types=*/{});

  WaitForNumberOfLoyaltyCards(0);
  WaitForNumberOfEntityInstances(0);
}

IN_PROC_BROWSER_TEST_P(SingleClientValuablesSyncTest,
                       AlternatingFullAndIncrementalUpdates) {
  const LoyaltyCard loyalty_card1 = CreateLoyaltyCard();
  const LoyaltyCard loyalty_card2 = CreateLoyaltyCard2();
  const EntityInstance vehicle1 =
      GetServerVehicleEntityInstanceWithRandomGuid();
  const EntityInstance vehicle2 =
      GetServerVehicleEntityInstanceWithRandomGuid();

  // Initial Full Update: setup server data with loyalty_card1 and vehicle1.
  SetFakeServerValuables({loyalty_card1, vehicle1});
  ASSERT_TRUE(SetupSync());
  ValuablesDataManager& vdm = GetValuablesDataManager();
  ASSERT_THAT(vdm.GetLoyaltyCards(), ElementsAre(loyalty_card1));
  EntityDataManager& edm = GetEntityDataManager();
  ASSERT_THAT(edm.GetEntityInstances(), UnorderedElementsAre(vehicle1));

  // Incremental Update #1: switch to incremental mode and inject loyalty_card2
  // and vehicle2.
  GetFakeServer()->SetUpdateMode(
      syncer::AUTOFILL_VALUABLE,
      fake_server::FakeServer::UpdateMode::kIncremental);
  InjectValuableEntityIncrementally(loyalty_card2);
  InjectValuableEntityIncrementally(vehicle2);
  WaitForNumberOfLoyaltyCards(2);
  WaitForNumberOfEntityInstances(2);
  EXPECT_THAT(vdm.GetLoyaltyCards(),
              UnorderedElementsAre(loyalty_card1, loyalty_card2));
  EXPECT_THAT(edm.GetEntityInstances(),
              UnorderedElementsAre(vehicle1, vehicle2));

  // Full Update #2: replace server data with loyalty_card2 and vehicle2 and
  // switch to full update mode.
  SetFakeServerValuables({loyalty_card2, vehicle2});
  GetFakeServer()->SetUpdateMode(syncer::AUTOFILL_VALUABLE,
                                 fake_server::FakeServer::UpdateMode::kFull);
  WaitForNumberOfLoyaltyCards(1);
  WaitForNumberOfEntityInstances(1);
  EXPECT_THAT(vdm.GetLoyaltyCards(), ElementsAre(loyalty_card2));
  EXPECT_THAT(edm.GetEntityInstances(), ElementsAre(vehicle2));

  // Incremental Update #2: switch back to incremental mode and inject
  // loyalty_card1 and vehicle1.
  GetFakeServer()->SetUpdateMode(
      syncer::AUTOFILL_VALUABLE,
      fake_server::FakeServer::UpdateMode::kIncremental);
  InjectValuableEntityIncrementally(loyalty_card1);
  InjectValuableEntityIncrementally(vehicle1);
  WaitForNumberOfLoyaltyCards(2);
  WaitForNumberOfEntityInstances(2);
  EXPECT_THAT(vdm.GetLoyaltyCards(),
              UnorderedElementsAre(loyalty_card1, loyalty_card2));
  EXPECT_THAT(edm.GetEntityInstances(),
              UnorderedElementsAre(vehicle1, vehicle2));
}

// Verifies that local entities are never uploaded to the sync server.
IN_PROC_BROWSER_TEST_P(SingleClientValuablesSyncTest, NotUploadLocalEntity) {
  ASSERT_TRUE(SetupSync());
  EntityDataManager& edm = GetEntityDataManager();
  ASSERT_THAT(edm.GetEntityInstances(), testing::IsEmpty());
  const EntityInstance vehicle =
      autofill::test::GetVehicleEntityInstanceWithRandomGuid();
  edm.AddOrUpdateEntityInstance(vehicle);
  EXPECT_TRUE(FakeServerSpecificsChecker(testing::IsEmpty()).Wait());
}

// Verifies that a new Wallet entity created locally is committed to the sync
// server.
IN_PROC_BROWSER_TEST_P(SingleClientValuablesSyncTest, UploadWalletEntity) {
  ASSERT_TRUE(SetupSync());
  EntityDataManager& edm = GetEntityDataManager();
  ASSERT_THAT(edm.GetEntityInstances(), testing::IsEmpty());
  const EntityInstance vehicle = GetServerVehicleEntityInstanceWithRandomGuid();
  edm.AddOrUpdateEntityInstance(vehicle);
  EXPECT_TRUE(
      FakeServerSpecificsChecker(UnorderedElementsAre(EqualsProto(
                                     AsAutofillValuableSpecifics(vehicle))))
          .Wait());

  const EntityInstance vehicle2 =
      GetServerVehicleEntityInstanceWithRandomGuid();
  edm.AddOrUpdateEntityInstance(vehicle2);
  EXPECT_TRUE(FakeServerSpecificsChecker(
                  UnorderedElementsAre(
                      EqualsProto(AsAutofillValuableSpecifics(vehicle)),
                      EqualsProto(AsAutofillValuableSpecifics(vehicle2))))
                  .Wait());
}

// Verifies that updating an existing Wallet entity locally correctly propagates
// that update to the sync server.
IN_PROC_BROWSER_TEST_P(SingleClientValuablesSyncTest,
                       UploadAndUpdateWalletEntity) {
  ASSERT_TRUE(SetupSync());
  EntityDataManager& edm = GetEntityDataManager();
  ASSERT_THAT(edm.GetEntityInstances(), testing::IsEmpty());
  const EntityInstance vehicle = GetServerVehicleEntityInstanceWithRandomGuid();
  edm.AddOrUpdateEntityInstance(vehicle);
  const EntityInstance updated_vehicle = vehicle.CopyWithUpdatedAttribute(
      MakeAttribute(AttributeTypeName::kVehicleModel, u"Q2"));
  // Update vehicle
  edm.AddOrUpdateEntityInstance(updated_vehicle);
  EXPECT_TRUE(FakeServerSpecificsChecker(
                  UnorderedElementsAre(EqualsProto(
                      AsAutofillValuableSpecifics(updated_vehicle))))
                  .Wait());
}

// Verifies that simultaneous local and remote changes are applied consistently.
// In this case, both updates are complementary. No common entity is affected.
IN_PROC_BROWSER_TEST_P(SingleClientValuablesSyncTest,
                       SimultaneousLocalAndRemoteChangeNoCommonEntity) {
  const EntityInstance vehicle1 =
      GetServerVehicleEntityInstanceWithRandomGuid();
  SetFakeServerValuables({vehicle1});
  ASSERT_TRUE(SetupSync());
  EntityDataManager& edm = GetEntityDataManager();
  WaitForNumberOfEntityInstances(1);
  ASSERT_THAT(edm.GetEntityInstances(), UnorderedElementsAre(vehicle1));

  // Commit some completely new data in the sync server.
  const EntityInstance vehicle2 =
      GetServerVehicleEntityInstanceWithRandomGuid();
  // This will trigger a sync update to the client. It overrides the `vehicle1`.
  SetFakeServerValuables({vehicle2});

  // Make a local change simultaneous with the server change.
  const EntityInstance vehicle3 =
      GetServerVehicleEntityInstanceWithRandomGuid();
  edm.AddOrUpdateEntityInstance(vehicle3);
  const EntityInstance updated_vehicle3 = vehicle3.CopyWithUpdatedAttribute(
      MakeAttribute(AttributeTypeName::kVehicleModel, u"Q2"));
  // Update vehicle
  edm.AddOrUpdateEntityInstance(updated_vehicle3);
  EXPECT_TRUE(
      FakeServerSpecificsChecker(
          Contains(EqualsProto(AsAutofillValuableSpecifics(updated_vehicle3))))
          .Wait());

  EXPECT_THAT(edm.GetEntityInstances(),
              UnorderedElementsAre(vehicle2, updated_vehicle3));
}

// Verifies that simultaneous local and remote changes are applied consistently.
// In this case, a local update is applied even if the same entity is received
// via a server update.
IN_PROC_BROWSER_TEST_P(SingleClientValuablesSyncTest,
                       SimultaneousLocalAndRemoteChangeCommonEntityNoConflict) {
  const EntityInstance vehicle1 =
      GetServerVehicleEntityInstanceWithRandomGuid();
  SetFakeServerValuables({vehicle1});
  ASSERT_TRUE(SetupSync());
  EntityDataManager& edm = GetEntityDataManager();
  WaitForNumberOfEntityInstances(1);
  ASSERT_THAT(edm.GetEntityInstances(), UnorderedElementsAre(vehicle1));

  // Commit some completely new data in the sync server.
  const EntityInstance vehicle2 =
      GetServerVehicleEntityInstanceWithRandomGuid();
  // This will trigger a sync update to the client.
  SetFakeServerValuables({vehicle1, vehicle2});

  // Make a local change simultaneous with the server change. Update vehicle.
  const EntityInstance updated_vehicle1 = vehicle1.CopyWithUpdatedAttribute(
      MakeAttribute(AttributeTypeName::kVehicleModel, u"Q2"));
  edm.AddOrUpdateEntityInstance(updated_vehicle1);
  EXPECT_TRUE(
      FakeServerSpecificsChecker(
          Contains(EqualsProto(AsAutofillValuableSpecifics(updated_vehicle1))))
          .Wait());

  EXPECT_THAT(edm.GetEntityInstances(),
              UnorderedElementsAre(updated_vehicle1, vehicle2));
}

// Verifies that simultaneous local and remote changes are applied consistently.
// In this case, conflicting updated versions of the same entity are received
// from the server and updated locally. The server entity must prevail.
IN_PROC_BROWSER_TEST_P(
    SingleClientValuablesSyncTest,
    SimultaneousLocalAndRemoteChangeCommonEntityWithConflict) {
  const EntityInstance vehicle1 =
      GetServerVehicleEntityInstanceWithRandomGuid();
  SetFakeServerValuables({vehicle1});
  ASSERT_TRUE(SetupSync());
  EntityDataManager& edm = GetEntityDataManager();
  WaitForNumberOfEntityInstances(1);
  ASSERT_THAT(edm.GetEntityInstances(), UnorderedElementsAre(vehicle1));

  // Commit some completely new data in the sync server.
  const EntityInstance vehicle2 =
      GetServerVehicleEntityInstanceWithRandomGuid();
  const EntityInstance server_updated_vehicle1 =
      vehicle1.CopyWithUpdatedAttribute(
          MakeAttribute(AttributeTypeName::kVehicleModel, u"A3"));
  // This will trigger a sync update to the client. It overrides the `vehicle1`.
  SetFakeServerValuables({server_updated_vehicle1, vehicle2});

  // Make a local change simultaneous with the server change. Update vehicle.
  const EntityInstance locally_updated_vehicle1 =
      vehicle1.CopyWithUpdatedAttribute(
          MakeAttribute(AttributeTypeName::kVehicleModel, u"Q2"));
  edm.AddOrUpdateEntityInstance(locally_updated_vehicle1);
  // The commit is never applied. The server update is preferred.
  WaitForNumberOfEntityInstances(2);
  EXPECT_THAT(edm.GetEntityInstances(),
              UnorderedElementsAre(server_updated_vehicle1, vehicle2));
}

// Verifies that server updates override client entities.
IN_PROC_BROWSER_TEST_P(SingleClientValuablesSyncTest,
                       ServerOverridesClientChanges) {
  ASSERT_TRUE(SetupSync());
  EntityDataManager& edm = GetEntityDataManager();
  ASSERT_THAT(edm.GetEntityInstances(), testing::IsEmpty());

  // Make a local change and commit it.
  const EntityInstance locally_committed_vehicle =
      GetServerVehicleEntityInstanceWithRandomGuid();
  edm.AddOrUpdateEntityInstance(locally_committed_vehicle);
  EXPECT_TRUE(FakeServerSpecificsChecker(
                  UnorderedElementsAre(EqualsProto(
                      AsAutofillValuableSpecifics(locally_committed_vehicle))))
                  .Wait());

  // Commit some completely new data in the sync server.
  const EntityInstance server_vehicle1 =
      GetServerVehicleEntityInstanceWithRandomGuid();
  const EntityInstance server_vehicle2 =
      GetServerVehicleEntityInstanceWithRandomGuid();
  // This will trigger a sync update to the client.
  SetFakeServerValuables({server_vehicle1, server_vehicle2});
  WaitForNumberOfEntityInstances(2);
  EXPECT_THAT(edm.GetEntityInstances(),
              UnorderedElementsAre(server_vehicle1, server_vehicle2));
}

INSTANTIATE_TEST_SUITE_P(,
                         SingleClientValuablesSyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

}  // namespace
