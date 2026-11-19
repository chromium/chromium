// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_package_sync_data_type_controller.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/arc/arc_package_syncable_service.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/experiences/arc/test/fake_app_instance.h"
#include "components/signin/public/identity_manager/account_managed_status_finder_outcome.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/sync_stop_metadata_fate.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/data_type_processor.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/arc_package_specifics.pb.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/service/configure_context.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_commit_queue.h"
#include "components/sync/test/mock_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using syncer::DataTypeController;
using testing::NiceMock;

DataTypeController::PreconditionContext MakePreconditionContext() {
  return DataTypeController::PreconditionContext(
      signin::AccountManagedStatusFinderOutcome::kConsumerGmail);
}

syncer::ConfigureContext MakeConfigureContext() {
  syncer::ConfigureContext context;
  context.authenticated_gaia_id = GaiaId("gaia_id");
  context.cache_guid = "cache_guid";
  return context;
}

syncer::UpdateResponseDataList MakeRemotePackageUpdates(
    std::string_view package_name,
    std::string_view server_id,
    int64_t response_version) {
  syncer::UpdateResponseData update;
  update.entity.id = server_id;
  update.entity.client_tag_hash =
      syncer::ClientTagHash::FromUnhashed(syncer::ARC_PACKAGE, package_name);
  update.entity.name = package_name;
  update.entity.specifics.mutable_arc_package()->set_package_name(package_name);
  update.response_version = response_version;
  syncer::UpdateResponseDataList updates;
  updates.push_back(std::move(update));
  return updates;
}

class ArcPackageSyncDataTypeControllerTest : public testing::Test {
 public:
  void SetUp() override {
    arc_app_test_.PreProfileSetUp();
    profile_ = std::make_unique<TestingProfile>();
    arc_app_test_.PostProfileSetUp(profile_.get());

    auto* syncable_service =
        arc::ArcPackageSyncableService::Get(profile_.get());
    ASSERT_TRUE(syncable_service);
    controller_ = std::make_unique<ArcPackageSyncDataTypeController>(
        syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
        syncable_service->AsWeakPtr(),
        /*dump_stack=*/base::DoNothing(), &mock_sync_service_, profile_.get());
  }

  void TearDown() override {
    controller_.reset();
    arc_app_test_.StopArcInstance();
    arc_app_test_.PreProfileTearDown();
    profile_.reset();
    arc_app_test_.PostProfileTearDown();
  }

 protected:
  std::unique_ptr<syncer::DataTypeActivationResponse> StartAndConnectSync() {
    base::test::TestFuture<const std::optional<syncer::ModelError>&>
        load_future;
    controller_->LoadModels(MakeConfigureContext(),
                            load_future.GetRepeatingCallback());
    EXPECT_EQ(std::nullopt, load_future.Get());
    EXPECT_EQ(DataTypeController::MODEL_LOADED, controller_->state());
    if (controller_->state() != DataTypeController::MODEL_LOADED) {
      return nullptr;
    }

    std::unique_ptr<syncer::DataTypeActivationResponse> activation_response =
        controller_->Connect();
    if (!activation_response || !activation_response->type_processor) {
      ADD_FAILURE() << "Connect() returned null response or processor";
      return nullptr;
    }
    EXPECT_EQ(DataTypeController::RUNNING, controller_->state());

    activation_response->type_processor->ConnectSync(
        std::make_unique<NiceMock<syncer::MockCommitQueue>>());
    base::RunLoop().RunUntilIdle();
    return activation_response;
  }

  content::BrowserTaskEnvironment task_environment_;
  ArcAppTest arc_app_test_;
  std::unique_ptr<TestingProfile> profile_;
  NiceMock<syncer::MockSyncService> mock_sync_service_;
  std::unique_ptr<ArcPackageSyncDataTypeController> controller_;
};

TEST_F(ArcPackageSyncDataTypeControllerTest,
       NotifiesSyncOnPackageRefreshAndAppConnectionClosed) {
  EXPECT_EQ(DataTypeController::PreconditionState::kMustStopAndKeepData,
            controller_->GetPreconditionState(MakePreconditionContext()));

  EXPECT_CALL(mock_sync_service_,
              DataTypePreconditionChanged(syncer::ARC_PACKAGE))
      .Times(1);
  arc_app_test_.app_instance()->SendRefreshPackageList({});
  testing::Mock::VerifyAndClearExpectations(&mock_sync_service_);
  EXPECT_EQ(DataTypeController::PreconditionState::kPreconditionsMet,
            controller_->GetPreconditionState(MakePreconditionContext()));

  // Closing the ARC app connection resets `package_list_initial_refreshed()`
  // to false and must notify Sync so it stops `ARC_PACKAGE`.
  EXPECT_CALL(mock_sync_service_,
              DataTypePreconditionChanged(syncer::ARC_PACKAGE))
      .Times(1);
  arc_app_test_.StopArcInstance();
  testing::Mock::VerifyAndClearExpectations(&mock_sync_service_);
  EXPECT_FALSE(
      arc_app_test_.arc_app_list_prefs()->package_list_initial_refreshed());
  EXPECT_EQ(DataTypeController::PreconditionState::kMustStopAndKeepData,
            controller_->GetPreconditionState(MakePreconditionContext()));

  // Reconnecting ARC alone must keep preconditions unmet until refresh.
  EXPECT_CALL(mock_sync_service_,
              DataTypePreconditionChanged(syncer::ARC_PACKAGE))
      .Times(0);
  arc_app_test_.RestartArcInstance();
  testing::Mock::VerifyAndClearExpectations(&mock_sync_service_);
  EXPECT_EQ(DataTypeController::PreconditionState::kMustStopAndKeepData,
            controller_->GetPreconditionState(MakePreconditionContext()));

  // Refreshing the package list restores preconditions and notifies Sync.
  EXPECT_CALL(mock_sync_service_,
              DataTypePreconditionChanged(syncer::ARC_PACKAGE))
      .Times(1);
  arc_app_test_.app_instance()->SendRefreshPackageList({});
  testing::Mock::VerifyAndClearExpectations(&mock_sync_service_);
  EXPECT_EQ(DataTypeController::PreconditionState::kPreconditionsMet,
            controller_->GetPreconditionState(MakePreconditionContext()));
}

TEST_F(ArcPackageSyncDataTypeControllerTest,
       StopsAndCancelsInFlightInitialUpdateOnAppConnectionClosed) {
  arc_app_test_.app_instance()->SendRefreshPackageList({});
  ASSERT_EQ(DataTypeController::PreconditionState::kPreconditionsMet,
            controller_->GetPreconditionState(MakePreconditionContext()));

  std::unique_ptr<syncer::DataTypeActivationResponse> activation_response =
      StartAndConnectSync();
  ASSERT_TRUE(activation_response);
  ASSERT_TRUE(activation_response->type_processor);

  constexpr char kRemotePackageName[] = "com.example.synced";
  sync_pb::DataTypeState data_type_state = activation_response->data_type_state;
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  activation_response->type_processor->OnUpdateReceived(
      data_type_state,
      MakeRemotePackageUpdates(kRemotePackageName, "server_id",
                               /*response_version=*/1),
      /*gc_directive=*/std::nullopt);

  // Mirror `DataTypeManagerImpl::DataTypePreconditionChanged()`: when the
  // controller notifies Sync on disconnect, stop the datatype synchronously so
  // the processor disconnects and cancels the queued `OnUpdateReceived()` task
  // before `ArcPackageSyncableService::MergeDataAndStartSyncing()` can run
  // with `package_list_initial_refreshed()` == false.
  EXPECT_CALL(mock_sync_service_,
              DataTypePreconditionChanged(syncer::ARC_PACKAGE))
      .WillOnce([this]() {
        ASSERT_EQ(DataTypeController::PreconditionState::kMustStopAndKeepData,
                  controller_->GetPreconditionState(MakePreconditionContext()));
        controller_->Stop(syncer::SyncStopMetadataFate::KEEP_METADATA,
                          base::DoNothing());
      });

  arc_app_test_.StopArcInstance();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller_->state());

  // Flush the queued `OnUpdateReceived()` task; it must be dropped rather than
  // invoking `MergeDataAndStartSyncing()` and hitting
  // `DCHECK(prefs_->package_list_initial_refreshed())`.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller_->state());
  auto* syncable_service = arc::ArcPackageSyncableService::Get(profile_.get());
  ASSERT_TRUE(syncable_service);
  EXPECT_FALSE(
      arc_app_test_.arc_app_list_prefs()->GetPackage(kRemotePackageName));
  EXPECT_FALSE(syncable_service->IsPackageSyncing(kRemotePackageName));
  testing::Mock::VerifyAndClearExpectations(&mock_sync_service_);

  // Reconnecting ARC and refreshing the package list allows initial sync to
  // restart cleanly and process the same update payload.
  arc_app_test_.RestartArcInstance();
  arc_app_test_.app_instance()->SendRefreshPackageList({});
  ASSERT_EQ(DataTypeController::PreconditionState::kPreconditionsMet,
            controller_->GetPreconditionState(MakePreconditionContext()));

  std::unique_ptr<syncer::DataTypeActivationResponse> reconnected_response =
      StartAndConnectSync();
  ASSERT_TRUE(reconnected_response);
  EXPECT_EQ(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED,
      reconnected_response->data_type_state.initial_sync_state());

  sync_pb::DataTypeState reconnected_state =
      reconnected_response->data_type_state;
  reconnected_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  reconnected_response->type_processor->OnUpdateReceived(
      reconnected_state,
      MakeRemotePackageUpdates(kRemotePackageName, "server_id",
                               /*response_version=*/1),
      /*gc_directive=*/std::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DataTypeController::RUNNING, controller_->state());
  // `FakeAppInstance::InstallPackage()` synchronously reports
  // `OnPackageAdded()`, installing the synced package into `ArcAppListPrefs`.
  EXPECT_TRUE(
      arc_app_test_.arc_app_list_prefs()->GetPackage(kRemotePackageName));
}

TEST_F(ArcPackageSyncDataTypeControllerTest,
       ProcessesIncrementalUpdateAfterDisconnectAndReconnect) {
  arc_app_test_.app_instance()->SendRefreshPackageList({});
  ASSERT_EQ(DataTypeController::PreconditionState::kPreconditionsMet,
            controller_->GetPreconditionState(MakePreconditionContext()));

  std::unique_ptr<syncer::DataTypeActivationResponse> activation_response =
      StartAndConnectSync();
  ASSERT_TRUE(activation_response);

  constexpr char kInitialPackageName[] = "com.example.initial";
  sync_pb::DataTypeState initial_state = activation_response->data_type_state;
  initial_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  activation_response->type_processor->OnUpdateReceived(
      initial_state,
      MakeRemotePackageUpdates(kInitialPackageName, "server_id_1",
                               /*response_version=*/1),
      /*gc_directive=*/std::nullopt);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(
      arc_app_test_.arc_app_list_prefs()->GetPackage(kInitialPackageName));

  // Disconnect ARC after initial sync completes; Sync stops with
  // `KEEP_METADATA`, preserving sync metadata and the bridge's started state.
  EXPECT_CALL(mock_sync_service_,
              DataTypePreconditionChanged(syncer::ARC_PACKAGE))
      .WillOnce([this]() {
        controller_->Stop(syncer::SyncStopMetadataFate::KEEP_METADATA,
                          base::DoNothing());
      });
  arc_app_test_.StopArcInstance();
  testing::Mock::VerifyAndClearExpectations(&mock_sync_service_);
  ASSERT_EQ(DataTypeController::NOT_RUNNING, controller_->state());

  // Reconnect ARC and deliver an incremental remote update; the service must
  // process it without failing with `kArcPackageSyncableServiceNotStarted`.
  arc_app_test_.RestartArcInstance();
  arc_app_test_.app_instance()->SendRefreshPackageList({});

  std::unique_ptr<syncer::DataTypeActivationResponse> reconnected_response =
      StartAndConnectSync();
  ASSERT_TRUE(reconnected_response);
  EXPECT_EQ(sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE,
            reconnected_response->data_type_state.initial_sync_state());

  constexpr char kIncrementalPackageName[] = "com.example.incremental";
  reconnected_response->type_processor->OnUpdateReceived(
      reconnected_response->data_type_state,
      MakeRemotePackageUpdates(kIncrementalPackageName, "server_id_2",
                               /*response_version=*/2),
      /*gc_directive=*/std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(DataTypeController::RUNNING, controller_->state());
  EXPECT_TRUE(
      arc_app_test_.arc_app_list_prefs()->GetPackage(kIncrementalPackageName));
}

TEST_F(ArcPackageSyncDataTypeControllerTest,
       PreconditionMustStopAndClearDataWhenPlayStoreDisabled) {
  arc_app_test_.app_instance()->SendRefreshPackageList({});
  ASSERT_EQ(DataTypeController::PreconditionState::kPreconditionsMet,
            controller_->GetPreconditionState(MakePreconditionContext()));

  EXPECT_CALL(mock_sync_service_,
              DataTypePreconditionChanged(syncer::ARC_PACKAGE))
      .Times(testing::AtLeast(1));
  arc::SetArcPlayStoreEnabledForProfile(profile_.get(), false);
  EXPECT_EQ(DataTypeController::PreconditionState::kMustStopAndClearData,
            controller_->GetPreconditionState(MakePreconditionContext()));
}

}  // namespace
