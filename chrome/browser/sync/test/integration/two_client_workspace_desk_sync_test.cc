// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/desk_template.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/uuid.h"
#include "chrome/browser/sync/desk_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/workspace_desk_helper.h"
#include "chrome/browser/ui/browser.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/desks_storage/core/desk_sync_service.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/workspace_desk_specifics.pb.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using ash::DeskTemplate;
using ash::DeskTemplateSource;
using ash::DeskTemplateType;
using desks_storage::DeskModel;
using desks_storage::DeskSyncService;
using sync_pb::WorkspaceDeskSpecifics;
using testing::Contains;

constexpr char kUuidFormat[] = "9e186d5a-502e-49ce-9ee1-00000000000%d";
constexpr char kNameFormat[] = "template %d";

WorkspaceDeskSpecifics CreateWorkspaceDeskSpecifics(int templateIndex,
                                                    base::Time created_time) {
  WorkspaceDeskSpecifics specifics;
  specifics.set_uuid(base::StringPrintf(kUuidFormat, templateIndex));
  specifics.set_name(base::StringPrintf(kNameFormat, templateIndex));
  specifics.set_created_time_windows_epoch_micros(
      created_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  return specifics;
}

// Waits for kUpToDate download status for WORKSPACE_DESK data type.
class DownloadStatusChecker : public SingleClientStatusChangeChecker {
 public:
  explicit DownloadStatusChecker(syncer::SyncServiceImpl* sync_service)
      : SingleClientStatusChangeChecker(sync_service) {}
  ~DownloadStatusChecker() override = default;

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for download status kUpToDate for WORKSPACE_DESK.";

    return service()->GetDownloadStatusFor(syncer::WORKSPACE_DESK) ==
           syncer::SyncService::DataTypeDownloadStatus::kUpToDate;
  }
};

class TwoClientWorkspaceDeskSyncTest : public SyncTest {
 public:
  TwoClientWorkspaceDeskSyncTest() : SyncTest(TWO_CLIENT) {}

  TwoClientWorkspaceDeskSyncTest(const TwoClientWorkspaceDeskSyncTest&) =
      delete;
  TwoClientWorkspaceDeskSyncTest& operator=(
      const TwoClientWorkspaceDeskSyncTest&) = delete;
  ~TwoClientWorkspaceDeskSyncTest() override = default;

  base::Time AdvanceAndGetTime(base::TimeDelta delta = base::Milliseconds(10)) {
    clock_.Advance(delta);
    return clock_.Now();
  }

 private:
  base::SimpleTestClock clock_;
};

IN_PROC_BROWSER_TEST_F(
    TwoClientWorkspaceDeskSyncTest,
    PRE_DownloadDeskTemplateWhenUpToDateFromOneClientToAnother) {
  ASSERT_TRUE(SetupSync());
}

IN_PROC_BROWSER_TEST_F(TwoClientWorkspaceDeskSyncTest,
                       DownloadDeskTemplateWhenUpToDateFromOneClientToAnother) {
  // Inject a test desk template to Sync.
  sync_pb::EntitySpecifics specifics;
  WorkspaceDeskSpecifics* desk = specifics.mutable_workspace_desk();
  desk->CopyFrom(CreateWorkspaceDeskSpecifics(1, AdvanceAndGetTime()));
  base::Uuid desk_1_uuid =
      base::Uuid::ParseCaseInsensitive(base::StringPrintf(kUuidFormat, 1));
  base::Uuid desk_2_uuid =
      base::Uuid::ParseCaseInsensitive(base::StringPrintf(kUuidFormat, 2));
  fake_server_->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          "desk_1", desk_1_uuid.AsLowercaseString(), specifics,
          /*creation_time=*/syncer::TimeToProtoTime(AdvanceAndGetTime()),
          /*last_modified_time=*/syncer::TimeToProtoTime(AdvanceAndGetTime())));

  ASSERT_TRUE(SetupClients());
  // Checks that client 1 was able to get the entry via the on `OnStateChanged`
  // from sync service.
  ASSERT_TRUE(DownloadStatusChecker(GetSyncService(0)).Wait());
  // Verify that the update has been actually downloaded.
  desks_storage::DeskModel* c1_desk_model =
      DeskSyncServiceFactory::GetForProfile(GetProfile(0))->GetDeskModel();
  EXPECT_THAT(c1_desk_model->GetAllEntryUuids(), Contains(desk_1_uuid));

  // Client 1 adds an entry and client 2 receives the entry correctly.
  desks_storage::DeskModel* c1_model =
      DeskSyncServiceFactory::GetForProfile(GetProfile(0))->GetDeskModel();
  ASSERT_TRUE(c1_model->IsSyncing());
  base::RunLoop loop;
  c1_model->AddOrUpdateEntry(
      std::make_unique<DeskTemplate>(desk_2_uuid, DeskTemplateSource::kUser,
                                     "desk_2", AdvanceAndGetTime(),
                                     DeskTemplateType::kTemplate),
      base::BindLambdaForTesting(
          [&](DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(DeskModel::AddOrUpdateEntryStatus::kOk, status);
            loop.Quit();
          }));
  loop.Run();
  EXPECT_TRUE(
      workspace_desk_helper::DeskUuidChecker(
          DeskSyncServiceFactory::GetForProfile(GetProfile(0)), desk_2_uuid)
          .Wait());
  // Verify that the update has been actually downloaded on client 2.
  EXPECT_TRUE(
      workspace_desk_helper::DeskUuidChecker(
          DeskSyncServiceFactory::GetForProfile(GetProfile(1)), desk_2_uuid)
          .Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientWorkspaceDeskSyncTest,
                       DeleteDeskTemplateIsSyncedAcrossBothClients) {
  sync_pb::EntitySpecifics specifics;
  WorkspaceDeskSpecifics* desk = specifics.mutable_workspace_desk();
  desk->CopyFrom(CreateWorkspaceDeskSpecifics(1, AdvanceAndGetTime()));
  base::Uuid desk_1_uuid =
      base::Uuid::ParseCaseInsensitive(base::StringPrintf(kUuidFormat, 1));
  fake_server_->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          "desk_1", desk_1_uuid.AsLowercaseString(), specifics,
          /*creation_time=*/syncer::TimeToProtoTime(AdvanceAndGetTime()),
          /*last_modified_time=*/syncer::TimeToProtoTime(AdvanceAndGetTime())));

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  // Make sure the template is on both client.
  ASSERT_TRUE(
      workspace_desk_helper::DeskUuidChecker(
          DeskSyncServiceFactory::GetForProfile(GetProfile(0)), desk_1_uuid)
          .Wait());

  ASSERT_TRUE(
      workspace_desk_helper::DeskUuidChecker(
          DeskSyncServiceFactory::GetForProfile(GetProfile(1)), desk_1_uuid)
          .Wait());

  desks_storage::DeskModel* model =
      DeskSyncServiceFactory::GetForProfile(GetProfile(0))->GetDeskModel();

  // Delete template 1.
  base::RunLoop loop;
  model->DeleteEntry(
      desk_1_uuid,
      base::BindLambdaForTesting([&](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(DeskModel::DeleteEntryStatus::kOk, status);
        loop.Quit();
      }));
  loop.Run();

  EXPECT_TRUE(
      workspace_desk_helper::DeskUuidDeletedChecker(
          DeskSyncServiceFactory::GetForProfile(GetProfile(0)), desk_1_uuid)
          .Wait());
  EXPECT_TRUE(
      workspace_desk_helper::DeskUuidDeletedChecker(
          DeskSyncServiceFactory::GetForProfile(GetProfile(1)), desk_1_uuid)
          .Wait());
}

}  // namespace
