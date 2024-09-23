// Copyright 2021 The Chromium Authors
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

class SingleClientWorkspaceDeskSyncTest : public SyncTest {
 public:
  SingleClientWorkspaceDeskSyncTest() : SyncTest(SINGLE_CLIENT) {
    kTestUuid1_ =
        base::Uuid::ParseCaseInsensitive(base::StringPrintf(kUuidFormat, 1));
  }

  SingleClientWorkspaceDeskSyncTest(const SingleClientWorkspaceDeskSyncTest&) =
      delete;
  SingleClientWorkspaceDeskSyncTest& operator=(
      const SingleClientWorkspaceDeskSyncTest&) = delete;
  ~SingleClientWorkspaceDeskSyncTest() override = default;

  base::Time AdvanceAndGetTime(base::TimeDelta delta = base::Milliseconds(10)) {
    clock_.Advance(delta);
    return clock_.Now();
  }

  void DisableDeskSync() {
    syncer::SyncService* service = GetSyncService(0);

      // Disable all OS types, including the desk sync type.
    service->GetUserSettings()->SetSelectedOsTypes(
        /*sync_all_os_types=*/false, syncer::UserSelectableOsTypeSet());

    ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  }

  base::Uuid kTestUuid1_;

 private:
  base::SimpleTestClock clock_;
};

IN_PROC_BROWSER_TEST_F(SingleClientWorkspaceDeskSyncTest,
                       DownloadDeskTemplateWhenSyncEnabled) {
  // Inject a test desk template to Sync.
  sync_pb::EntitySpecifics specifics;
  WorkspaceDeskSpecifics* desk = specifics.mutable_workspace_desk();
  desk->CopyFrom(CreateWorkspaceDeskSpecifics(1, AdvanceAndGetTime()));

  fake_server_->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          "non_unique_name", kTestUuid1_.AsLowercaseString(), specifics,
          /*creation_time=*/syncer::TimeToProtoTime(AdvanceAndGetTime()),
          /*last_modified_time=*/syncer::TimeToProtoTime(AdvanceAndGetTime())));

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  syncer::SyncService* sync_service = GetSyncService(0);
  ASSERT_TRUE(sync_service->GetActiveDataTypes().Has(syncer::WORKSPACE_DESK));

  // Check the test desk template is downloaded.
  EXPECT_TRUE(
      workspace_desk_helper::DeskUuidChecker(
          DeskSyncServiceFactory::GetForProfile(GetProfile(0)), kTestUuid1_)
          .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientWorkspaceDeskSyncTest,
                       PRE_DownloadDeskTemplateWhenUpToDate) {
  ASSERT_TRUE(SetupSync());
}

IN_PROC_BROWSER_TEST_F(SingleClientWorkspaceDeskSyncTest,
                       DownloadDeskTemplateWhenUpToDate) {
  // Inject a test desk template to Sync.
  sync_pb::EntitySpecifics specifics;
  WorkspaceDeskSpecifics* desk = specifics.mutable_workspace_desk();
  desk->CopyFrom(CreateWorkspaceDeskSpecifics(1, AdvanceAndGetTime()));

  fake_server_->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          "non_unique_name", kTestUuid1_.AsLowercaseString(), specifics,
          /*creation_time=*/syncer::TimeToProtoTime(AdvanceAndGetTime()),
          /*last_modified_time=*/syncer::TimeToProtoTime(AdvanceAndGetTime())));

  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(DownloadStatusChecker(GetSyncService(0)).Wait());

  // Verify that the update has been actually downloaded.
  desks_storage::DeskModel* desk_model =
      DeskSyncServiceFactory::GetForProfile(GetProfile(0))->GetDeskModel();
  EXPECT_THAT(desk_model->GetAllEntryUuids(), Contains(kTestUuid1_));
}

IN_PROC_BROWSER_TEST_F(SingleClientWorkspaceDeskSyncTest, IsReady) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  EXPECT_TRUE(workspace_desk_helper::DeskModelReadyChecker(
                  DeskSyncServiceFactory::GetForProfile(GetProfile(0)))
                  .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientWorkspaceDeskSyncTest, DeleteDeskTemplate) {
  sync_pb::EntitySpecifics specifics;
  WorkspaceDeskSpecifics* desk = specifics.mutable_workspace_desk();
  desk->CopyFrom(CreateWorkspaceDeskSpecifics(1, AdvanceAndGetTime()));

  fake_server_->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          "non_unique_name", kTestUuid1_.AsLowercaseString(), specifics,
          /*creation_time=*/syncer::TimeToProtoTime(AdvanceAndGetTime()),
          /*last_modified_time=*/syncer::TimeToProtoTime(AdvanceAndGetTime())));

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(
      workspace_desk_helper::DeskUuidChecker(
          DeskSyncServiceFactory::GetForProfile(GetProfile(0)), kTestUuid1_)
          .Wait());

  desks_storage::DeskModel* model =
      DeskSyncServiceFactory::GetForProfile(GetProfile(0))->GetDeskModel();

  // Delete template 1.
  base::RunLoop loop;
  model->DeleteEntry(
      kTestUuid1_,
      base::BindLambdaForTesting([&](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(DeskModel::DeleteEntryStatus::kOk, status);
        loop.Quit();
      }));
  loop.Run();

  EXPECT_TRUE(
      workspace_desk_helper::DeskUuidDeletedChecker(
          DeskSyncServiceFactory::GetForProfile(GetProfile(0)), kTestUuid1_)
          .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientWorkspaceDeskSyncTest,
                       ShouldAllowAddTemplateLocallyWhenSyncIsDisabled) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  DisableDeskSync();

  EXPECT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::WORKSPACE_DESK));

  desks_storage::DeskModel* model =
      DeskSyncServiceFactory::GetForProfile(GetProfile(0))->GetDeskModel();

  ASSERT_FALSE(model->IsSyncing());

  base::RunLoop loop;
  model->AddOrUpdateEntry(
      std::make_unique<DeskTemplate>(kTestUuid1_, DeskTemplateSource::kUser,
                                     "template 1", AdvanceAndGetTime(),
                                     DeskTemplateType::kTemplate),
      base::BindLambdaForTesting(
          [&](DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(DeskModel::AddOrUpdateEntryStatus::kOk, status);
            loop.Quit();
          }));
  loop.Run();

  DeskSyncService* service =
      DeskSyncServiceFactory::GetForProfile(GetProfile(0));
  // Check the test desk template is added.
  EXPECT_TRUE(
      workspace_desk_helper::DeskUuidChecker(service, kTestUuid1_).Wait());

  // There should be exactly one desk template.
  EXPECT_EQ(1u, service->GetDeskModel()->GetEntryCount());
}

}  // namespace
