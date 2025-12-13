// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <vector>

#include "base/strings/strcat.h"
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
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/valuables_data_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_sync_util.h"
#include "components/autofill/core/browser/webdata/valuables/valuables_sync_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/sync/base/features.h"
#include "components/sync/protocol/entity_data.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

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
using sync_datatype_helper::test;
using testing::ElementsAre;
using testing::UnorderedElementsAre;

namespace {

EntityInstance GetServerVehicleEntityInstance(
    autofill::test::VehicleOptions options = {}) {
  options.nickname = "";
  options.date_modified = {};
  options.use_date = {};
  options.record_type = EntityInstance::RecordType::kServerWallet;
  options.are_attributes_read_only =
      EntityInstance::AreAttributesReadOnly(false);
  return autofill::test::GetVehicleEntityInstance(options);
}

EntityInstance GetServerVehicleEntityInstanceWithRandomGuid() {
  return autofill::test::GetVehicleEntityInstanceWithRandomGuid(
      {.nickname = "",
       .date_modified = {},
       .use_date = {},
       .record_type = EntityInstance::RecordType::kServerWallet,
       .are_attributes_read_only =
           EntityInstance::AreAttributesReadOnly(false)});
}

EntityInstance GetFlightReservationEntityInstanceWithRandomGuid() {
  return autofill::test::GetFlightReservationEntityInstanceWithRandomGuid(
      {.nickname = "",
       .date_modified = {},
       .use_date = {},
       .record_type = EntityInstance::RecordType::kServerWallet,
       // Flight reservations are read-only.
       .are_attributes_read_only =
           EntityInstance::AreAttributesReadOnly(true)});
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
      autofill::CreateSpecificsFromEntityInstance(entity_instance);
  return entity;
}

// Since the sync server operates in terms of entity specifics, this helper
// function converts a given `entity_instance` to the equivalent
// `AutofillValuableSpecifics`.
sync_pb::AutofillValuableSpecifics AsAutofillValuableSpecifics(
    const EntityInstance& entity_instance) {
  return autofill::CreateSpecificsFromEntityInstance(entity_instance);
}

// Helper class to wait until the fake server's AutofillValuableSpecifics match
// a given predicate. Unfortunately, since protos don't have an equality
// operator, the comparisons are based on the `base::test::EqualsProto()`
// representation of the specifics.
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

class SingleClientValuableSyncTestBase : public SyncTest {
 public:
  SingleClientValuableSyncTestBase() : SyncTest(SINGLE_CLIENT) {}
  SingleClientValuableSyncTestBase(const SingleClientValuableSyncTestBase&) =
      delete;
  SingleClientValuableSyncTestBase& operator=(
      const SingleClientValuableSyncTestBase&) = delete;

  ~SingleClientValuableSyncTestBase() override = default;

  ValuablesDataManager* GetValuablesDataManager(int index) {
    return ValuablesDataManagerFactory::GetForProfile(
        test()->GetProfile(index));
  }

 protected:
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

  void WaitForNumberOfLoyaltyCards(size_t expected_count,
                                   ValuablesDataManager* vdm) {
    while (vdm->GetLoyaltyCards().size() != expected_count ||
           vdm->HasPendingQueries()) {
      ValuablesDataChangedWaiter(vdm).Wait();
    }
  }
  base::test::ScopedFeatureList feature_list_;
};

class SingleClientValuablesSyncTest
    : public SingleClientValuableSyncTestBase,
      public testing::WithParamInterface<
          std::tuple<bool, SyncTest::SetupSyncMode>> {
 public:
  SingleClientValuablesSyncTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        autofill::features::kAutofillEnableLoyaltyCardsFilling,
        syncer::kSyncAutofillLoyaltyCard};
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      enabled_features.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
    }
    std::vector<base::test::FeatureRef> disabled_features;
    if (IsValuablesInProfileDBEnabled()) {
      enabled_features.push_back(syncer::kSyncMoveValuablesToProfileDb);
    } else {
      disabled_features.push_back(syncer::kSyncMoveValuablesToProfileDb);
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  ~SingleClientValuablesSyncTest() override = default;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return std::get<1>(GetParam());
  }

  bool IsValuablesInProfileDBEnabled() const { return std::get<0>(GetParam()); }
};

// Valuables data should get loaded on initial sync.
IN_PROC_BROWSER_TEST_P(SingleClientValuablesSyncTest, InitialSync) {
  const LoyaltyCard loyalty_card = CreateLoyaltyCard();
  GetFakeServer()->SetValuableData({LoyaltyCardToSyncEntity(loyalty_card)});
  ASSERT_TRUE(SetupSync());
  ValuablesDataManager* vdm = GetValuablesDataManager(0);
  ASSERT_NE(nullptr, vdm);
  // Make sure the data & metadata is in the DB.
  EXPECT_THAT(vdm->GetLoyaltyCards(), ElementsAre(loyalty_card));
}

// ChromeOS does not support late signin after profile creation, so the test
// below does not apply, at least in the current form.
#if !BUILDFLAG(IS_CHROMEOS)
// Valuables data should get cleared from the database when the user signs out.
IN_PROC_BROWSER_TEST_P(SingleClientValuablesSyncTest, ClearOnSignOut) {
  const LoyaltyCard loyalty_card = CreateLoyaltyCard();
  GetFakeServer()->SetValuableData({LoyaltyCardToSyncEntity(loyalty_card)});
  ASSERT_TRUE(SetupSync());
  ValuablesDataManager* vdm = GetValuablesDataManager(0);
  ASSERT_NE(nullptr, vdm);
  // Make sure the data & metadata is in the DB.
  EXPECT_THAT(vdm->GetLoyaltyCards(), ElementsAre(loyalty_card));

  // Signout, the data & metadata should be gone.
  GetClient(0)->SignOutPrimaryAccount();
  WaitForNumberOfLoyaltyCards(0, vdm);

  EXPECT_EQ(0uL, vdm->GetLoyaltyCards().size());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Valuables data should get cleared from the database when the user enters the
// sync paused state (e.g. persistent auth error).
IN_PROC_BROWSER_TEST_P(SingleClientValuablesSyncTest, ClearOnSyncPaused) {
  const LoyaltyCard loyalty_card = CreateLoyaltyCard();
  GetFakeServer()->SetValuableData({LoyaltyCardToSyncEntity(loyalty_card)});
  ASSERT_TRUE(SetupSync());
  ValuablesDataManager* vdm = GetValuablesDataManager(0);
  ASSERT_NE(nullptr, vdm);
  // Make sure the data & metadata is in the DB.
  EXPECT_THAT(vdm->GetLoyaltyCards(), ElementsAre(loyalty_card));

  // Enter sync paused state, the data & metadata should be gone.
  EnterSyncPausedStateForPrimaryAccount();
  WaitForNumberOfLoyaltyCards(0, vdm);
  EXPECT_EQ(0uL, vdm->GetLoyaltyCards().size());

  // When exiting the sync paused state, the data should be redownloaded.
  ExitSyncPausedStateForPrimaryAccount();
  WaitForNumberOfLoyaltyCards(1, vdm);
  EXPECT_EQ(1uL, vdm->GetLoyaltyCards().size());
}

// Valuables are not using incremental updates. Make sure existing data gets
// replaced when synced down.
IN_PROC_BROWSER_TEST_P(SingleClientValuablesSyncTest,
                       NewSyncDataShouldReplaceExistingData) {
  const LoyaltyCard loyalty_card = CreateLoyaltyCard();
  GetFakeServer()->SetValuableData({LoyaltyCardToSyncEntity(loyalty_card)});
  ASSERT_TRUE(SetupSync());
  ValuablesDataManager* vdm = GetValuablesDataManager(0);
  ASSERT_NE(nullptr, vdm);
  EXPECT_THAT(vdm->GetLoyaltyCards(), ElementsAre(loyalty_card));

  ValuablesDataChangedWaiter waiter(vdm);
  // Put some completely new data in the sync server.
  const LoyaltyCard loyalty_card2 = CreateLoyaltyCard2();
  GetFakeServer()->SetValuableData({LoyaltyCardToSyncEntity(loyalty_card2)});
  waiter.Wait();
  EXPECT_THAT(vdm->GetLoyaltyCards(), ElementsAre(loyalty_card2));
}

IN_PROC_BROWSER_TEST_P(SingleClientValuablesSyncTest,
                       ClearOnDisablePaymentsSync) {
  const LoyaltyCard loyalty_card = CreateLoyaltyCard();
  GetFakeServer()->SetValuableData({LoyaltyCardToSyncEntity(loyalty_card)});
  ASSERT_TRUE(SetupSync());
  ValuablesDataManager* vdm = GetValuablesDataManager(0);
  ASSERT_NE(nullptr, vdm);
  EXPECT_THAT(vdm->GetLoyaltyCards(), ElementsAre(loyalty_card));

  // Turn off payments sync, the data & metadata should be gone.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kPayments));

  WaitForNumberOfLoyaltyCards(0, vdm);
  EXPECT_THAT(vdm->GetLoyaltyCards(), testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_P(SingleClientValuablesSyncTest,
                       ClearOnDisableWalletAutofill) {
  const LoyaltyCard loyalty_card = CreateLoyaltyCard();
  GetFakeServer()->SetValuableData({LoyaltyCardToSyncEntity(loyalty_card)});
  ASSERT_TRUE(SetupSync());
  ValuablesDataManager* vdm = GetValuablesDataManager(0);
  ASSERT_NE(nullptr, vdm);
  EXPECT_THAT(vdm->GetLoyaltyCards(), ElementsAre(loyalty_card));

  // Turn off the wallet autofill pref, the data & metadata should be gone as a
  // side effect of the wallet data type controller noticing.
  GetSyncService(0)->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, /*types=*/{});

  WaitForNumberOfLoyaltyCards(0, vdm);
  EXPECT_THAT(vdm->GetLoyaltyCards(), testing::IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SingleClientValuablesSyncTest,
    testing::Combine(testing::Bool(), GetSyncTestModes()),
    [](const testing::TestParamInfo<SingleClientValuablesSyncTest::ParamType>&
           info) {
      return base::StrCat({std::get<0>(info.param) ? "ValuablesInProfileDB"
                                                   : "ValuablesInAccountDB",
                           testing::PrintToString(std::get<1>(info.param))});
    });

// DB migration tests for valuables.
class MigrateValuableDatabasesSyncTest
    : public SingleClientValuableSyncTestBase,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  MigrateValuableDatabasesSyncTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        autofill::features::kAutofillEnableLoyaltyCardsFilling,
        syncer::kSyncAutofillLoyaltyCard};

    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      enabled_features.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
    }

    std::vector<base::test::FeatureRef> disabled_features;
    if (GetTestPreCount() == 0 || GetTestPreCount() == 2) {
      disabled_features.push_back(syncer::kSyncMoveValuablesToProfileDb);
    } else {
      enabled_features.push_back(syncer::kSyncMoveValuablesToProfileDb);
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetUpOnMainThread() override {
    SingleClientValuableSyncTestBase::SetUpOnMainThread();
    GetFakeServer()->SetValuableData({LoyaltyCardToSyncEntity(loyalty_card1_),
                                      LoyaltyCardToSyncEntity(loyalty_card2_)});
  }
  ~MigrateValuableDatabasesSyncTest() override = default;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

 protected:
  const LoyaltyCard loyalty_card1_ = CreateLoyaltyCard();
  const LoyaltyCard loyalty_card2_ = CreateLoyaltyCard2();
};

// With `kSyncMoveValuablesToProfileDb` disabled, valuables are loaded normally
// from the account DB.
IN_PROC_BROWSER_TEST_P(MigrateValuableDatabasesSyncTest,
                       PRE_PRE_MigrateValuablesDB) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(SetupSync());
  ValuablesDataManager* vdm = GetValuablesDataManager(0);
  ASSERT_NE(nullptr, vdm);
  // Make sure the data & metadata is in the DB.
  WaitForNumberOfLoyaltyCards(2, vdm);
  EXPECT_THAT(vdm->GetLoyaltyCards(),
              UnorderedElementsAre(loyalty_card1_, loyalty_card2_));
}

// With `kSyncMoveValuablesToProfileDb` enabled, valuables storage is migrated
// to the profile DB. The DB starts fresh and sync downloads the latest set of
// valuables.
IN_PROC_BROWSER_TEST_P(MigrateValuableDatabasesSyncTest,
                       PRE_MigrateValuablesDB) {
  ASSERT_TRUE(SetupClients());
  ValuablesDataManager* vdm = GetValuablesDataManager(0);
  ASSERT_NE(nullptr, vdm);
  WaitForNumberOfLoyaltyCards(2, vdm);
  // Make sure the data & metadata is in the DB.
  EXPECT_THAT(vdm->GetLoyaltyCards(),
              UnorderedElementsAre(loyalty_card1_, loyalty_card2_));
}

// With `kSyncMoveValuablesToProfileDb` disabled again, valuables are loaded
// from the account DB again.
IN_PROC_BROWSER_TEST_P(MigrateValuableDatabasesSyncTest, MigrateValuablesDB) {
  ASSERT_TRUE(SetupClients());
  ValuablesDataManager* vdm = GetValuablesDataManager(0);
  ASSERT_NE(nullptr, vdm);
  WaitForNumberOfLoyaltyCards(2, vdm);
  // Make sure the data & metadata is in the DB.
  EXPECT_THAT(vdm->GetLoyaltyCards(),
              UnorderedElementsAre(loyalty_card1_, loyalty_card2_));
}

INSTANTIATE_TEST_SUITE_P(,
                         MigrateValuableDatabasesSyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

class SingleClientEntityValuablesSyncTest
    : public SingleClientValuableSyncTestBase,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SingleClientEntityValuablesSyncTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        syncer::kSyncAutofillLoyaltyCard, syncer::kSyncMoveValuablesToProfileDb,
        syncer::kSyncWalletFlightReservations,
        syncer::kSyncWalletVehicleRegistrations};
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      enabled_features.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
    }
    feature_list_.InitWithFeatures(enabled_features, {});
  }

  ~SingleClientEntityValuablesSyncTest() override = default;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }
  EntityDataManager* GetEntityDataManager(int index) {
    return AutofillEntityDataManagerFactory::GetForProfile(
        test()->GetProfile(index));
  }

 protected:
  void WaitForNumberOfEntityInstancesCards(size_t expected_count,
                                           EntityDataManager* edm) {
    while (edm->GetEntityInstances().size() != expected_count ||
           edm->HasPendingQueries()) {
      EntityDataChangedWaiter(edm).Wait();
    }
  }
};

// Entities data should get loaded on initial sync.
IN_PROC_BROWSER_TEST_P(SingleClientEntityValuablesSyncTest, InitialSync) {
  const EntityInstance vehicle = GetServerVehicleEntityInstanceWithRandomGuid();
  const EntityInstance flight =
      GetFlightReservationEntityInstanceWithRandomGuid();
  GetFakeServer()->SetValuableData({EntityInstanceToSyncEntity(vehicle),
                                    EntityInstanceToSyncEntity(flight)});
  ASSERT_TRUE(SetupSync());
  EntityDataManager* edm = GetEntityDataManager(0);
  ASSERT_NE(nullptr, edm);
  // Make sure the data & metadata is in the DB.
  EXPECT_THAT(edm->GetEntityInstances(), UnorderedElementsAre(vehicle, flight));
}

// ChromeOS does not support late signin after profile creation, so the test
// below does not apply, at least in the current form.
#if !BUILDFLAG(IS_CHROMEOS)
// Wallet entities should get cleared from the entity database when the user
// signs out.
IN_PROC_BROWSER_TEST_P(SingleClientEntityValuablesSyncTest, ClearOnSignOut) {
  const EntityInstance vehicle = GetServerVehicleEntityInstanceWithRandomGuid();
  const EntityInstance flight =
      GetFlightReservationEntityInstanceWithRandomGuid();
  GetFakeServer()->SetValuableData({EntityInstanceToSyncEntity(vehicle),
                                    EntityInstanceToSyncEntity(flight)});
  ASSERT_TRUE(SetupSync());
  EntityDataManager* edm = GetEntityDataManager(0);
  ASSERT_NE(nullptr, edm);
  // Make sure the data & metadata is in the DB.
  EXPECT_THAT(edm->GetEntityInstances(), UnorderedElementsAre(vehicle, flight));

  // Signout, the data & metadata should be gone.
  GetClient(0)->SignOutPrimaryAccount();
  WaitForNumberOfEntityInstancesCards(0, edm);
  EXPECT_EQ(0uL, edm->GetEntityInstances().size());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Wallet should get cleared from the database when the user enters the
// sync paused state (e.g. persistent auth error).
IN_PROC_BROWSER_TEST_P(SingleClientEntityValuablesSyncTest, ClearOnSyncPaused) {
  const EntityInstance vehicle = GetServerVehicleEntityInstanceWithRandomGuid();
  const EntityInstance flight =
      GetFlightReservationEntityInstanceWithRandomGuid();
  GetFakeServer()->SetValuableData({EntityInstanceToSyncEntity(vehicle),
                                    EntityInstanceToSyncEntity(flight)});
  ASSERT_TRUE(SetupSync());
  EntityDataManager* edm = GetEntityDataManager(0);
  ASSERT_NE(nullptr, edm);
  // Make sure the data & metadata is in the DB.
  EXPECT_THAT(edm->GetEntityInstances(), UnorderedElementsAre(vehicle, flight));

  // Enter sync paused state, the data & metadata should be gone.
  EnterSyncPausedStateForPrimaryAccount();
  WaitForNumberOfEntityInstancesCards(0, edm);
  EXPECT_EQ(0uL, edm->GetEntityInstances().size());

  // When exiting the sync paused state, the data should be redownloaded.
  ExitSyncPausedStateForPrimaryAccount();
  WaitForNumberOfEntityInstancesCards(2, edm);
  EXPECT_EQ(2uL, edm->GetEntityInstances().size());
}

// Valuables are not using incremental updates. Make sure existing entities gets
// replaced when synced down.
IN_PROC_BROWSER_TEST_P(SingleClientEntityValuablesSyncTest,
                       NewSyncDataShouldReplaceExistingData) {
  const EntityInstance vehicle = GetServerVehicleEntityInstanceWithRandomGuid();
  const EntityInstance flight =
      GetFlightReservationEntityInstanceWithRandomGuid();
  GetFakeServer()->SetValuableData({EntityInstanceToSyncEntity(vehicle),
                                    EntityInstanceToSyncEntity(flight)});
  ASSERT_TRUE(SetupSync());
  EntityDataManager* edm = GetEntityDataManager(0);
  ASSERT_NE(nullptr, edm);
  EXPECT_THAT(edm->GetEntityInstances(), UnorderedElementsAre(vehicle, flight));

  // Put some completely new data in the sync server.
  const EntityInstance vehicle2 =
      GetServerVehicleEntityInstanceWithRandomGuid();
  const EntityInstance flight2 =
      GetFlightReservationEntityInstanceWithRandomGuid();
  GetFakeServer()->SetValuableData({EntityInstanceToSyncEntity(vehicle2),
                                    EntityInstanceToSyncEntity(flight2)});
  EntityDataChangedWaiter(edm).Wait();
  EXPECT_THAT(edm->GetEntityInstances(),
              UnorderedElementsAre(vehicle2, flight2));
}

// Wallet entities should get cleared from the entity database when the user
// disables payments sync.
IN_PROC_BROWSER_TEST_P(SingleClientEntityValuablesSyncTest,
                       ClearOnDisablePaymentsSync) {
  const EntityInstance vehicle = GetServerVehicleEntityInstanceWithRandomGuid();
  const EntityInstance flight =
      GetFlightReservationEntityInstanceWithRandomGuid();
  GetFakeServer()->SetValuableData({EntityInstanceToSyncEntity(vehicle),
                                    EntityInstanceToSyncEntity(flight)});
  ASSERT_TRUE(SetupSync());
  EntityDataManager* edm = GetEntityDataManager(0);
  ASSERT_NE(nullptr, edm);
  // Make sure the data & metadata is in the DB.
  EXPECT_THAT(edm->GetEntityInstances(), UnorderedElementsAre(vehicle, flight));

  // Turn off payments sync, the data & metadata should be gone.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kPayments));

  WaitForNumberOfEntityInstancesCards(0, edm);
  EXPECT_EQ(0uL, edm->GetEntityInstances().size());
}

// Verifies that local entities are never uploaded to the sync server.
IN_PROC_BROWSER_TEST_P(SingleClientEntityValuablesSyncTest,
                       NotUploadLocalEntity) {
  ASSERT_TRUE(SetupSync());
  EntityDataManager* edm = GetEntityDataManager(0);
  ASSERT_NE(nullptr, edm);
  ASSERT_THAT(edm->GetEntityInstances(), testing::IsEmpty());
  const EntityInstance vehicle =
      autofill::test::GetVehicleEntityInstanceWithRandomGuid();
  edm->AddOrUpdateEntityInstance(vehicle);
  EXPECT_TRUE(FakeServerSpecificsChecker(testing::IsEmpty()).Wait());
}

// Verifies that a new wallet entity created locally is successfully uploaded to
// the sync server.
IN_PROC_BROWSER_TEST_P(SingleClientEntityValuablesSyncTest,
                       UploadWalletEntity) {
  ASSERT_TRUE(SetupSync());
  EntityDataManager* edm = GetEntityDataManager(0);
  ASSERT_NE(nullptr, edm);
  ASSERT_THAT(edm->GetEntityInstances(), testing::IsEmpty());
  const EntityInstance vehicle = GetServerVehicleEntityInstanceWithRandomGuid();
  edm->AddOrUpdateEntityInstance(vehicle);
  EXPECT_TRUE(
      FakeServerSpecificsChecker(UnorderedElementsAre(base::test::EqualsProto(
                                     AsAutofillValuableSpecifics(vehicle))))
          .Wait());

  const EntityInstance vehicle2 =
      GetServerVehicleEntityInstanceWithRandomGuid();
  edm->AddOrUpdateEntityInstance(vehicle2);
  EXPECT_TRUE(
      FakeServerSpecificsChecker(
          UnorderedElementsAre(
              base::test::EqualsProto(AsAutofillValuableSpecifics(vehicle)),
              base::test::EqualsProto(AsAutofillValuableSpecifics(vehicle2))))
          .Wait());
}

// Verifies that updating an existing wallet entity locally correctly propagates
// that update to the sync server.
IN_PROC_BROWSER_TEST_P(SingleClientEntityValuablesSyncTest,
                       UploadAndUpdateWalletEntity) {
  ASSERT_TRUE(SetupSync());
  EntityDataManager* edm = GetEntityDataManager(0);
  ASSERT_NE(nullptr, edm);
  ASSERT_THAT(edm->GetEntityInstances(), testing::IsEmpty());
  const EntityInstance vehicle = GetServerVehicleEntityInstance();
  edm->AddOrUpdateEntityInstance(vehicle);
  const EntityInstance updated_vehicle =
      GetServerVehicleEntityInstance({.model = u"Q2"});
  // Update vehicle
  edm->AddOrUpdateEntityInstance(updated_vehicle);
  EXPECT_TRUE(FakeServerSpecificsChecker(
                  UnorderedElementsAre(base::test::EqualsProto(
                      AsAutofillValuableSpecifics(updated_vehicle))))
                  .Wait());
}

// Verifies that simultaneous local and remote changes are applied consistently.
// In this case, both updates are complementary. No common entity is affected.
IN_PROC_BROWSER_TEST_P(SingleClientEntityValuablesSyncTest,
                       SimultaneousLocalAndRemoteChangeNoCommonEntity) {
  const EntityInstance initial_vehicle =
      GetServerVehicleEntityInstanceWithRandomGuid();
  GetFakeServer()->SetValuableData(
      {EntityInstanceToSyncEntity(initial_vehicle)});
  ASSERT_TRUE(SetupSync());
  EntityDataManager* edm = GetEntityDataManager(0);
  ASSERT_NE(nullptr, edm);
  WaitForNumberOfEntityInstancesCards(1, edm);
  ASSERT_THAT(edm->GetEntityInstances(), UnorderedElementsAre(initial_vehicle));

  // Put some completely new data in the sync server.
  const EntityInstance vehicle2 =
      GetServerVehicleEntityInstanceWithRandomGuid();
  const EntityInstance flight2 =
      GetFlightReservationEntityInstanceWithRandomGuid();

  // This will trigger a sync update to the client. It overrides the
  // `initial_vehicle`.
  GetFakeServer()->SetValuableData({EntityInstanceToSyncEntity(vehicle2),
                                    EntityInstanceToSyncEntity(flight2)});

  // Make a local change simultaneous with the server change.
  const EntityInstance vehicle3 = GetServerVehicleEntityInstance();
  edm->AddOrUpdateEntityInstance(vehicle3);
  const EntityInstance updated_vehicle3 =
      GetServerVehicleEntityInstance({.model = u"Q2"});
  // Update vehicle
  edm->AddOrUpdateEntityInstance(updated_vehicle3);
  EXPECT_TRUE(FakeServerSpecificsChecker(
                  UnorderedElementsAre(base::test::EqualsProto(
                      AsAutofillValuableSpecifics(updated_vehicle3))))
                  .Wait());

  EXPECT_THAT(edm->GetEntityInstances(),
              UnorderedElementsAre(vehicle2, flight2, updated_vehicle3));
}

// Verifies that simultaneous local and remote changes are applied consistently.
// In this case, a local update is applied even if the same entity is received
// via a server update.
IN_PROC_BROWSER_TEST_P(SingleClientEntityValuablesSyncTest,
                       SimultaneousLocalAndRemoteChangeCommonEntityNoConflict) {
  const EntityInstance server_vehicle = GetServerVehicleEntityInstance();
  GetFakeServer()->SetValuableData(
      {EntityInstanceToSyncEntity(server_vehicle)});
  ASSERT_TRUE(SetupSync());
  EntityDataManager* edm = GetEntityDataManager(0);
  ASSERT_NE(nullptr, edm);
  WaitForNumberOfEntityInstancesCards(1, edm);
  ASSERT_THAT(edm->GetEntityInstances(), UnorderedElementsAre(server_vehicle));

  // Put some completely new data in the sync server.
  const EntityInstance flight =
      GetFlightReservationEntityInstanceWithRandomGuid();

  // This will trigger a sync update to the client. It overrides the
  // `server_vehicle`.
  GetFakeServer()->SetValuableData({EntityInstanceToSyncEntity(server_vehicle),
                                    EntityInstanceToSyncEntity(flight)});

  // Make a local change simultaneous with the server change. Update vehicle.
  const EntityInstance updated_server_vehicle =
      GetServerVehicleEntityInstance({.model = u"Q2"});
  edm->AddOrUpdateEntityInstance(updated_server_vehicle);
  EXPECT_TRUE(FakeServerSpecificsChecker(
                  UnorderedElementsAre(base::test::EqualsProto(
                      AsAutofillValuableSpecifics(updated_server_vehicle))))
                  .Wait());

  EXPECT_THAT(edm->GetEntityInstances(),
              UnorderedElementsAre(updated_server_vehicle, flight));
}

// Verifies that simultaneous local and remote changes are applied consistently.
// In this case, conflicting updated versions of the same entity are received
// from the server and updated locally. The server entity must prevail.
IN_PROC_BROWSER_TEST_P(
    SingleClientEntityValuablesSyncTest,
    SimultaneousLocalAndRemoteChangeCommonEntityWithConflict) {
  const EntityInstance server_vehicle = GetServerVehicleEntityInstance();
  GetFakeServer()->SetValuableData(
      {EntityInstanceToSyncEntity(server_vehicle)});
  ASSERT_TRUE(SetupSync());
  EntityDataManager* edm = GetEntityDataManager(0);
  ASSERT_NE(nullptr, edm);
  WaitForNumberOfEntityInstancesCards(1, edm);
  ASSERT_THAT(edm->GetEntityInstances(), UnorderedElementsAre(server_vehicle));

  // Put some completely new data in the sync server.
  const EntityInstance flight =
      GetFlightReservationEntityInstanceWithRandomGuid();

  const EntityInstance updated_server_vehicle =
      GetServerVehicleEntityInstance({.model = u"A3"});
  // This will trigger a sync update to the client. It overrides the
  // `server_vehicle`.
  GetFakeServer()->SetValuableData(
      {EntityInstanceToSyncEntity(updated_server_vehicle),
       EntityInstanceToSyncEntity(flight)});

  // Make a local change simultaneous with the server change. Update vehicle.
  const EntityInstance updated_local_vehicle =
      GetServerVehicleEntityInstance({.model = u"Q2"});
  edm->AddOrUpdateEntityInstance(updated_local_vehicle);
  // The commit is never applied. The server update is preferred.
  WaitForNumberOfEntityInstancesCards(2, edm);
  EXPECT_THAT(edm->GetEntityInstances(),
              UnorderedElementsAre(updated_server_vehicle, flight));
}

// Verifies that server updates override client entities.
IN_PROC_BROWSER_TEST_P(SingleClientEntityValuablesSyncTest,
                       ServerOverridesClientChanges) {
  ASSERT_TRUE(SetupSync());
  EntityDataManager* edm = GetEntityDataManager(0);
  ASSERT_NE(nullptr, edm);
  ASSERT_THAT(edm->GetEntityInstances(), testing::IsEmpty());

  // Make a local change and commit it.
  const EntityInstance local_vehicle =
      GetServerVehicleEntityInstanceWithRandomGuid();
  edm->AddOrUpdateEntityInstance(local_vehicle);
  EXPECT_TRUE(FakeServerSpecificsChecker(
                  UnorderedElementsAre(base::test::EqualsProto(
                      AsAutofillValuableSpecifics(local_vehicle))))
                  .Wait());

  // Put some completely new data in the sync server.
  const EntityInstance server_vehicle =
      GetServerVehicleEntityInstanceWithRandomGuid();
  const EntityInstance server_flight =
      GetFlightReservationEntityInstanceWithRandomGuid();

  // This will trigger a sync update to the client.
  GetFakeServer()->SetValuableData({EntityInstanceToSyncEntity(server_vehicle),
                                    EntityInstanceToSyncEntity(server_flight)});
  EntityDataChangedWaiter(edm).Wait();
  EXPECT_THAT(edm->GetEntityInstances(),
              UnorderedElementsAre(server_vehicle, server_flight));
}

INSTANTIATE_TEST_SUITE_P(,
                         SingleClientEntityValuablesSyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

}  // namespace
