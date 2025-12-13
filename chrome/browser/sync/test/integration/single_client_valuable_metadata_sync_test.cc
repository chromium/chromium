// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/protobuf_matchers.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_sync_util.h"
#include "components/sync/base/features.h"
#include "components/sync/protocol/autofill_valuable_metadata_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using autofill::AutofillEntityDataManagerFactory;
using autofill::EntityDataChangedWaiter;
using autofill::EntityDataManager;
using autofill::EntityInstance;
using autofill::test::GetVehicleEntityInstance;
using autofill::test::GetVehicleEntityInstanceWithRandomGuid;
using autofill::test::VehicleOptions;
using base::test::EqualsProto;
using sync_datatype_helper::test;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

EntityInstance CreateServerVehicleEntityInstance(VehicleOptions options = {}) {
  options.nickname = "";
  options.record_type = EntityInstance::RecordType::kServerWallet;
  return GetVehicleEntityInstanceWithRandomGuid(options);
}

sync_pb::SyncEntity EntityInstanceToSyncEntity(
    const EntityInstance& entity_instance) {
  sync_pb::SyncEntity entity;
  entity.set_name(*entity_instance.guid());
  entity.set_id_string(*entity_instance.guid());
  entity.set_version(0);  // Will be overridden by the fake server.
  entity.set_ctime(12345);
  entity.set_mtime(12345);
  sync_pb::AutofillValuableSpecifics* valuable_specifics =
      entity.mutable_specifics()->mutable_autofill_valuable();
  *valuable_specifics =
      autofill::CreateSpecificsFromEntityInstance(entity_instance);
  return entity;
}

sync_pb::AutofillValuableMetadataSpecifics AsAutofillValuableMetadataSpecifics(
    const EntityInstance::EntityMetadata& metadata) {
  return autofill::CreateSpecificsFromEntityMetadata(metadata);
}

class FakeServerValuableMetadataChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  using Matcher =
      testing::Matcher<std::vector<sync_pb::AutofillValuableMetadataSpecifics>>;

  explicit FakeServerValuableMetadataChecker(const Matcher& matcher)
      : matcher_(matcher) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    std::vector<sync_pb::AutofillValuableMetadataSpecifics> specifics;
    for (const sync_pb::SyncEntity& entity :
         fake_server()->GetSyncEntitiesByDataType(
             syncer::AUTOFILL_VALUABLE_METADATA)) {
      specifics.push_back(entity.specifics().autofill_valuable_metadata());
    }
    testing::StringMatchResultListener listener;
    bool matches = testing::ExplainMatchResult(matcher_, specifics, &listener);
    *os << listener.str();
    return matches;
  }

 private:
  const Matcher matcher_;
};

class SingleClientValuableMetadataSyncTest : public SyncTest {
 public:
  SingleClientValuableMetadataSyncTest() : SyncTest(SINGLE_CLIENT) {
    feature_list_.InitWithFeatures({syncer::kSyncAutofillValuableMetadata,
                                    syncer::kSyncWalletFlightReservations,
                                    syncer::kSyncWalletVehicleRegistrations},
                                   {});
  }

  ~SingleClientValuableMetadataSyncTest() override = default;

  EntityDataManager* GetEntityDataManager() {
    return AutofillEntityDataManagerFactory::GetForProfile(
        test()->GetProfile(0));
  }

  base::span<const autofill::EntityInstance> GetEntityInstances() {
    return GetEntityDataManager()->GetEntityInstances();
  }

  std::vector<EntityInstance::EntityMetadata> GetMetadataEntries() {
    std::vector<EntityInstance::EntityMetadata> all_metadata;
    for (const EntityInstance& entity : GetEntityInstances()) {
      all_metadata.push_back(entity.metadata());
    }
    return all_metadata;
  }

  void InjectEntitiesToServer(const std::vector<EntityInstance>& entities) {
    std::vector<sync_pb::SyncEntity> valuable_entities;
    valuable_entities.reserve(entities.size());
    for (const EntityInstance& entity : entities) {
      valuable_entities.push_back(EntityInstanceToSyncEntity(entity));
    }
    GetFakeServer()->SetValuableData(valuable_entities);
  }

  void InjectEntityMetadataToServer(
      const EntityInstance::EntityMetadata& metadata) {
    sync_pb::AutofillValuableMetadataSpecifics specifics =
        CreateSpecificsFromEntityMetadata(metadata);
    sync_pb::EntitySpecifics entity_specifics;
    *entity_specifics.mutable_autofill_valuable_metadata() = specifics;
    GetFakeServer()->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/"autofill-entity-metadata",
            /*client_tag=*/
            specifics.valuable_id(), entity_specifics,
            /*creation_time=*/0, /*last_modified_time=*/0));
  }

 protected:
  void WaitForNumberOfEntityInstances(size_t expected_count,
                                      EntityDataManager* edm) {
    while (edm->GetEntityInstances().size() != expected_count ||
           edm->HasPendingQueries()) {
      EntityDataChangedWaiter(edm).Wait();
    }
  }
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that when valuable data (e.g., a vehicle) and its metadata are
// already on the server, the client correctly downloads and associates them
// during the initial sync.
IN_PROC_BROWSER_TEST_F(SingleClientValuableMetadataSyncTest, InitialSync) {
  EntityInstance server_vehicle = CreateServerVehicleEntityInstance();
  EntityInstance::EntityMetadata server_metadata =
      EntityInstance::EntityMetadata{
          .guid = server_vehicle.guid(),
          .date_modified = base::Time::FromSecondsSinceUnixEpoch(400),
          .use_count = 5,
          .use_date = base::Time::FromSecondsSinceUnixEpoch(500),
      };

  InjectEntitiesToServer({server_vehicle});
  InjectEntityMetadataToServer(server_metadata);

  ASSERT_TRUE(SetupSync());
  ASSERT_EQ(1u, GetEntityInstances().size());
  // Update the  `server_vehicle` with the expected `server_metadata`.
  server_vehicle.set_metadata(server_metadata);

  // Make sure the data & metadata is in the DB.
  EXPECT_THAT(GetEntityInstances(), ElementsAre(server_vehicle));
  EXPECT_THAT(GetMetadataEntries(), ElementsAre(server_metadata));
}

// Checks the incremental update scenario, ensuring that if metadata for a
// valuable entity arrives from the server before the entity itself, the client
// correctly associates them once the entity arrives.
IN_PROC_BROWSER_TEST_F(SingleClientValuableMetadataSyncTest,
                       IncrementalUpdatesMetadataArrivesFirst) {
  EntityInstance server_vehicle = CreateServerVehicleEntityInstance();
  EntityInstance::EntityMetadata server_metadata =
      EntityInstance::EntityMetadata{
          .guid = server_vehicle.guid(),
          .date_modified = base::Time::FromSecondsSinceUnixEpoch(400),
          .use_count = 5,
          .use_date = base::Time::FromSecondsSinceUnixEpoch(500),
      };
  InjectEntityMetadataToServer(server_metadata);
  ASSERT_TRUE(SetupSync());
  InjectEntitiesToServer({server_vehicle});

  EntityDataManager* edm = GetEntityDataManager();
  WaitForNumberOfEntityInstances(1, edm);
  ASSERT_EQ(1u, GetEntityInstances().size());
  // Update the  `server_vehicle` with the expected `server_metadata`.
  server_vehicle.set_metadata(server_metadata);

  // Make sure the data & metadata is in the DB.
  EXPECT_THAT(GetEntityInstances(), ElementsAre(server_vehicle));
  EXPECT_THAT(GetMetadataEntries(), ElementsAre(server_metadata));
}

// Verifies that when a new valuable entity with metadata is created on the
// client, its metadata is correctly uploaded to the sync server.
IN_PROC_BROWSER_TEST_F(SingleClientValuableMetadataSyncTest,
                       UploadMetadataForNewEntries) {
  ASSERT_TRUE(SetupSync());
  const EntityInstance vehicle = CreateServerVehicleEntityInstance({
      .date_modified = base::Time::FromSecondsSinceUnixEpoch(400),
      .use_date = base::Time::FromSecondsSinceUnixEpoch(400),
      .use_count = 5,
  });

  EntityDataManager* edm = GetEntityDataManager();
  edm->AddOrUpdateEntityInstance(vehicle);
  WaitForNumberOfEntityInstances(1, edm);
  EXPECT_TRUE(FakeServerValuableMetadataChecker(
                  UnorderedElementsAre(EqualsProto(
                      AsAutofillValuableMetadataSpecifics(vehicle.metadata()))))
                  .Wait());
}

// Ensures that when a user interacts with a valuable entity, the client updates
// the entity's metadata (e.g., `use_count`, `use_date`) and uploads these
// changes to the sync server.
IN_PROC_BROWSER_TEST_F(SingleClientValuableMetadataSyncTest,
                       UploadRecordEntityUsed) {
  ASSERT_TRUE(SetupSync());
  const EntityInstance vehicle = CreateServerVehicleEntityInstance({
      .date_modified = base::Time::FromSecondsSinceUnixEpoch(400),
      .use_date = base::Time::FromSecondsSinceUnixEpoch(400),
      .use_count = 5,
  });

  EntityDataManager* edm = GetEntityDataManager();
  edm->AddOrUpdateEntityInstance(vehicle);
  WaitForNumberOfEntityInstances(1, edm);
  EXPECT_TRUE(FakeServerValuableMetadataChecker(
                  UnorderedElementsAre(EqualsProto(
                      AsAutofillValuableMetadataSpecifics(vehicle.metadata()))))
                  .Wait());

  base::Time last_used = base::Time::FromSecondsSinceUnixEpoch(500);
  edm->RecordEntityUsed(vehicle.guid(), last_used);
  EntityInstance::EntityMetadata updated_metadata =
      EntityInstance::EntityMetadata{
          .guid = vehicle.guid(),
          .date_modified = base::Time::FromSecondsSinceUnixEpoch(400),
          .use_count = 6,
          .use_date = last_used,
      };
  WaitForNumberOfEntityInstances(1, edm);
  EXPECT_TRUE(FakeServerValuableMetadataChecker(
                  UnorderedElementsAre(EqualsProto(
                      AsAutofillValuableMetadataSpecifics(updated_metadata))))
                  .Wait());
}

// Simulates the deletion of a valuable entity on the server and verifies that
// the client correctly removes the corresponding entity and its metadata
// locally. It also confirms that the metadata for the deleted entity is removed
// from the server.
IN_PROC_BROWSER_TEST_F(SingleClientValuableMetadataSyncTest,
                       DeleteMetadataFromSyncedEntitiesInIncrementalChanges) {
  const EntityInstance vehicle1 = CreateServerVehicleEntityInstance();
  const EntityInstance vehicle2 = CreateServerVehicleEntityInstance();
  EntityInstance::EntityMetadata vehicle1_metadata =
      EntityInstance::EntityMetadata{
          .guid = vehicle1.guid(),
          .date_modified = base::Time::FromSecondsSinceUnixEpoch(400),
          .use_count = 5,
          .use_date = base::Time::FromSecondsSinceUnixEpoch(500)};
  EntityInstance::EntityMetadata vehicle2_metadata =
      EntityInstance::EntityMetadata{
          .guid = vehicle2.guid(),
          .date_modified = base::Time::FromSecondsSinceUnixEpoch(800),
          .use_count = 7,
          .use_date = base::Time::FromSecondsSinceUnixEpoch(900)};
  InjectEntitiesToServer({vehicle1, vehicle2});
  InjectEntityMetadataToServer(vehicle1_metadata);
  InjectEntityMetadataToServer(vehicle2_metadata);

  ASSERT_TRUE(SetupSync());
  EntityDataManager* edm = GetEntityDataManager();
  EXPECT_THAT(GetMetadataEntries(),
              UnorderedElementsAre(vehicle1_metadata, vehicle2_metadata));

  GetFakeServer()->SetValuableData({EntityInstanceToSyncEntity(vehicle2)});
  WaitForNumberOfEntityInstances(1, edm);

  EXPECT_TRUE(FakeServerValuableMetadataChecker(
                  UnorderedElementsAre(EqualsProto(
                      AsAutofillValuableMetadataSpecifics(vehicle2_metadata))))
                  .Wait());
}

// Ensures that metadata for local-only entities (not synced from the server) is
// not uploaded to the sync server, even when these entities are used or
// modified.
IN_PROC_BROWSER_TEST_F(SingleClientValuableMetadataSyncTest,
                       LocalEntityInstanceMetadataIsNotUploaded) {
  ASSERT_TRUE(SetupSync());
  const EntityInstance local_vehicle = GetVehicleEntityInstanceWithRandomGuid();
  EntityDataManager* edm = GetEntityDataManager();
  edm->AddOrUpdateEntityInstance(local_vehicle);
  WaitForNumberOfEntityInstances(1, edm);
  EXPECT_THAT(GetMetadataEntries(), ElementsAre(local_vehicle.metadata()));
  EXPECT_TRUE(FakeServerValuableMetadataChecker(IsEmpty()).Wait());

  base::Time last_used = base::Time::FromSecondsSinceUnixEpoch(500);
  edm->RecordEntityUsed(local_vehicle.guid(), last_used);
  WaitForNumberOfEntityInstances(1, edm);
  EXPECT_TRUE(FakeServerValuableMetadataChecker(IsEmpty()).Wait());
}

#if !BUILDFLAG(IS_CHROMEOS)
// Verifies that signing out of the primary account clears all valuable entity
// data and metadata from the local database. This test is disabled on ChromeOS.
IN_PROC_BROWSER_TEST_F(SingleClientValuableMetadataSyncTest, ClearOnSignOut) {
  const EntityInstance server_vehicle = CreateServerVehicleEntityInstance();
  InjectEntitiesToServer({server_vehicle});

  ASSERT_TRUE(SetupSync());
  EntityDataManager* edm = GetEntityDataManager();
  ASSERT_EQ(1u, GetEntityInstances().size());

  GetClient(0)->SignOutPrimaryAccount();
  WaitForNumberOfEntityInstances(0, edm);
  EXPECT_THAT(GetMetadataEntries(), IsEmpty());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Ensures that disabling the "Payments" sync toggle clears all valuable entity
// data and metadata from the local database.
IN_PROC_BROWSER_TEST_F(SingleClientValuableMetadataSyncTest,
                       ClearOnDisablePaymentsSync) {
  const EntityInstance vehicle = CreateServerVehicleEntityInstance();
  InjectEntitiesToServer({vehicle});

  ASSERT_TRUE(SetupSync());
  EntityDataManager* edm = GetEntityDataManager();
  // Make sure the data & metadata is in the DB.
  ASSERT_EQ(1u, GetEntityInstances().size());

  // Turn off payments sync, the data & metadata should be gone.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kPayments));

  WaitForNumberOfEntityInstances(0, edm);
  EXPECT_THAT(GetMetadataEntries(), IsEmpty());
}

// Tests that in case of a metadata conflict, the client's changes are preserved
// (client wins).
IN_PROC_BROWSER_TEST_F(SingleClientValuableMetadataSyncTest,
                       ConflictResolutionClientWins) {
  const EntityInstance vehicle = CreateServerVehicleEntityInstance(
      {.date_modified = base::Time::FromSecondsSinceUnixEpoch(400),
       .use_date = base::Time::FromSecondsSinceUnixEpoch(400),
       .use_count = 5});
  InjectEntitiesToServer({vehicle});
  InjectEntityMetadataToServer(vehicle.metadata());

  ASSERT_TRUE(SetupSync());
  EntityDataManager* edm = GetEntityDataManager();
  ASSERT_EQ(1u, GetEntityInstances().size());
  ASSERT_THAT(GetMetadataEntries(), ElementsAre(vehicle.metadata()));

  // Simulate a local usage update.
  base::Time last_used = base::Time::FromSecondsSinceUnixEpoch(500);
  edm->RecordEntityUsed(vehicle.guid(), last_used);

  // Simulate a concurrent update from the server with a conflicting use_count.
  EntityInstance::EntityMetadata conflicting_metadata = {
      .guid = vehicle.guid(),
      .date_modified = base::Time::FromSecondsSinceUnixEpoch(400),
      .use_count = 10,
      .use_date = base::Time::FromSecondsSinceUnixEpoch(600)};
  InjectEntityMetadataToServer(conflicting_metadata);

  while (GetMetadataEntries().empty() ||
         GetMetadataEntries()[0].use_count != conflicting_metadata.use_count) {
    EntityDataChangedWaiter(edm).Wait();
  }
  // Check that server data wins.
  EXPECT_THAT(GetMetadataEntries(), ElementsAre(conflicting_metadata));
}

// Verifies that re-enabling the "Payments" sync toggle correctly re-downloads
// valuable entity data and metadata.
IN_PROC_BROWSER_TEST_F(SingleClientValuableMetadataSyncTest,
                       ReenablingPaymentsSyncDownloadsData) {
  const EntityInstance vehicle = CreateServerVehicleEntityInstance(
      {.date_modified = base::Time::FromSecondsSinceUnixEpoch(400),
       .use_date = base::Time::FromSecondsSinceUnixEpoch(500),
       .use_count = 5});
  InjectEntitiesToServer({vehicle});
  InjectEntityMetadataToServer(vehicle.metadata());

  ASSERT_TRUE(SetupSync());
  EntityDataManager* edm = GetEntityDataManager();
  ASSERT_EQ(1u, GetEntityInstances().size());

  // Turn off payments sync, the data & metadata should be gone.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kPayments));
  WaitForNumberOfEntityInstances(0, edm);
  ASSERT_THAT(GetMetadataEntries(), IsEmpty());

  // Turn payments sync back on.
  ASSERT_TRUE(GetClient(0)->EnableSelectableType(
      syncer::UserSelectableType::kPayments));
  WaitForNumberOfEntityInstances(1, edm);

  // The data and metadata should be restored.
  EntityInstance expected_vehicle = vehicle;
  EXPECT_THAT(GetEntityInstances(), ElementsAre(expected_vehicle));
  EXPECT_THAT(GetMetadataEntries(), ElementsAre(vehicle.metadata()));
}

// Verifies that the client correctly processes a metadata-only update from the
// server for an existing valuable entity.
IN_PROC_BROWSER_TEST_F(SingleClientValuableMetadataSyncTest,
                       ServerInitiatedMetadataUpdate) {
  const EntityInstance vehicle = CreateServerVehicleEntityInstance();
  EntityInstance::EntityMetadata initial_metadata =
      EntityInstance::EntityMetadata{
          .guid = vehicle.guid(),
          .date_modified = base::Time::FromSecondsSinceUnixEpoch(400),
          .use_count = 5,
          .use_date = base::Time::FromSecondsSinceUnixEpoch(500)};
  InjectEntitiesToServer({vehicle});
  InjectEntityMetadataToServer(initial_metadata);

  ASSERT_TRUE(SetupSync());
  EntityDataManager* edm = GetEntityDataManager();
  EXPECT_THAT(GetMetadataEntries(), ElementsAre(initial_metadata));

  // Now, update the metadata on the server.
  EntityInstance::EntityMetadata updated_metadata =
      EntityInstance::EntityMetadata{
          .guid = vehicle.guid(),
          .date_modified = base::Time::FromSecondsSinceUnixEpoch(600),
          .use_count = 10,
          .use_date = base::Time::FromSecondsSinceUnixEpoch(700)};
  InjectEntityMetadataToServer(updated_metadata);
  // Wait for the client to receive and apply the metadata update.
  while (GetMetadataEntries().empty() ||
         GetMetadataEntries()[0].use_count != updated_metadata.use_count) {
    EntityDataChangedWaiter(edm).Wait();
  }

  EXPECT_THAT(GetMetadataEntries(), ElementsAre(updated_metadata));
}

}  // namespace
