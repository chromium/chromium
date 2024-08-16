// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/sync_invalidations_service_factory.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/device_info_helper.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/invalidations/sync_invalidations_service.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/service/glue/sync_transport_data_prefs.h"
#include "components/sync/test/bookmark_entity_builder.h"
#include "components/sync/test/entity_builder_factory.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/device_info_util.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using bookmarks_helper::AddFolder;
using bookmarks_helper::GetBookmarkBarNode;
using bookmarks_helper::ServerBookmarksEqualityChecker;
using syncer::DataType;
using testing::AllOf;
using testing::Contains;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Not;
using testing::NotNull;
using testing::SizeIs;

constexpr char kSyncedBookmarkURL[] = "http://www.mybookmark.com";
constexpr char kSyncedBookmarkTitle[] = "Title";

syncer::DataTypeSet DefaultInterestedDataTypes() {
  return Difference(syncer::ProtocolTypes(), syncer::CommitOnlyTypes());
}

// Injects a new bookmark into the |fake_server| and returns a UUID of a created
// entity. Note that this trigges an invalidations from the server.
base::Uuid InjectSyncedBookmark(fake_server::FakeServer* fake_server) {
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(kSyncedBookmarkTitle);
  std::unique_ptr<syncer::LoopbackServerEntity> bookmark_entity =
      bookmark_builder.BuildBookmark(GURL(kSyncedBookmarkURL));
  base::Uuid bookmark_uuid = base::Uuid::ParseLowercase(
      bookmark_entity->GetSpecifics().bookmark().guid());
  fake_server->InjectEntity(std::move(bookmark_entity));
  return bookmark_uuid;
}

MATCHER_P(HasBeenUpdatedAfter, last_updated_timestamp, "") {
  return arg.specifics().device_info().last_updated_timestamp() >
         last_updated_timestamp;
}

MATCHER_P(HasCacheGuid, expected_cache_guid, "") {
  return arg.specifics().device_info().cache_guid() == expected_cache_guid;
}

MATCHER_P(InterestedDataTypesAre, expected_data_types, "") {
  syncer::DataTypeSet data_types;
  for (const int field_number : arg.specifics()
                                    .device_info()
                                    .invalidation_fields()
                                    .interested_data_type_ids()) {
    DataType data_type =
        syncer::GetDataTypeFromSpecificsFieldNumber(field_number);
    if (!syncer::IsRealDataType(data_type)) {
      return false;
    }
    data_types.Put(data_type);
  }
  return data_types == expected_data_types;
}

MATCHER_P(InterestedDataTypesContain, expected_data_type, "") {
  syncer::DataTypeSet data_types;
  for (const int field_number : arg.specifics()
                                    .device_info()
                                    .invalidation_fields()
                                    .interested_data_type_ids()) {
    DataType data_type =
        syncer::GetDataTypeFromSpecificsFieldNumber(field_number);
    if (!syncer::IsRealDataType(data_type)) {
      return false;
    }
    data_types.Put(data_type);
  }
  return data_types.Has(expected_data_type);
}

MATCHER(HasInstanceIdToken, "") {
  return arg.specifics()
      .device_info()
      .invalidation_fields()
      .has_instance_id_token();
}

MATCHER_P(HasInstanceIdToken, expected_token, "") {
  return arg.specifics()
             .device_info()
             .invalidation_fields()
             .instance_id_token() == expected_token;
}

sync_pb::DataTypeProgressMarker GetProgressMarkerForType(
    const sync_pb::GetUpdatesMessage& gu_message,
    DataType type) {
  for (const sync_pb::DataTypeProgressMarker& progress_marker :
       gu_message.from_progress_marker()) {
    if (progress_marker.data_type_id() ==
        syncer::GetSpecificsFieldNumberFromDataType(type)) {
      return progress_marker;
    }
  }
  return sync_pb::DataTypeProgressMarker();
}

class GetUpdatesFailureChecker : public SingleClientStatusChangeChecker {
 public:
  explicit GetUpdatesFailureChecker(syncer::SyncServiceImpl* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    syncer::SyncCycleSnapshot last_cycle_snapshot =
        service()->GetLastCycleSnapshotForDebugging();

    *os << "Waiting for GetUpdates error, current result: \""
        << last_cycle_snapshot.model_neutral_state()
               .last_download_updates_result.ToString()
        << "\".";

    return last_cycle_snapshot.model_neutral_state()
               .last_download_updates_result.type() !=
           syncer::SyncerError::Type::kSuccess;
  }
};

// Waits for a successful GetUpdates request containing a notification for the
// given |type|.
class NotificationHintChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  explicit NotificationHintChecker(DataType type) : type_(type) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for a notification hint for "
        << syncer::DataTypeToDebugString(type_) << ".";

    sync_pb::ClientToServerMessage last_get_updates;
    if (!fake_server()->GetLastGetUpdatesMessage(&last_get_updates)) {
      *os << "No GetUpdates request received yet.";
    }

    sync_pb::DataTypeProgressMarker progress_marker =
        GetProgressMarkerForType(last_get_updates.get_updates(), type_);
    if (progress_marker.data_type_id() !=
        syncer::GetSpecificsFieldNumberFromDataType(type_)) {
      *os << "Last GetUpdates does not contain progress marker for "
          << syncer::DataTypeToDebugString(type_) << ".";
    }

    return !progress_marker.get_update_triggers().notification_hint().empty();
  }

 private:
  const DataType type_;
};

// This class helps to count the number of GU_TRIGGER events for the |type|
// since the object has been created.
class GetUpdatesTriggeredObserver : public fake_server::FakeServer::Observer {
 public:
  GetUpdatesTriggeredObserver(fake_server::FakeServer* fake_server,
                              DataType type)
      : fake_server_(fake_server), type_(type) {
    fake_server_->AddObserver(this);
  }

  ~GetUpdatesTriggeredObserver() override {
    fake_server_->RemoveObserver(this);
  }

  void OnSuccessfulGetUpdates() override {
    sync_pb::ClientToServerMessage message;
    fake_server_->GetLastGetUpdatesMessage(&message);

    if (message.get_updates().get_updates_origin() !=
        sync_pb::SyncEnums::GU_TRIGGER) {
      return;
    }
    for (const sync_pb::DataTypeProgressMarker& progress_marker :
         message.get_updates().from_progress_marker()) {
      if (progress_marker.data_type_id() !=
          syncer::GetSpecificsFieldNumberFromDataType(type_)) {
        continue;
      }
      if (progress_marker.get_update_triggers().datatype_refresh_nudges() > 0) {
        num_nudged_get_updates_for_data_type_++;
      }
    }
  }

  size_t num_nudged_get_updates_for_data_type() const {
    return num_nudged_get_updates_for_data_type_;
  }

 private:
  const raw_ptr<fake_server::FakeServer> fake_server_;
  const DataType type_;

  size_t num_nudged_get_updates_for_data_type_ = 0;
};

sync_pb::DeviceInfoSpecifics CreateDeviceInfoSpecifics(
    const std::string& cache_guid,
    syncer::DataTypeSet interested_data_types,
    const std::string& fcm_registration_token) {
  sync_pb::DeviceInfoSpecifics specifics;
  specifics.set_cache_guid(cache_guid);
  specifics.set_client_name("client name");
  specifics.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_LINUX);
  specifics.set_sync_user_agent("user agent");
  specifics.set_chrome_version("chrome version");
  specifics.set_signin_scoped_device_id("scoped device id");
  specifics.set_last_updated_timestamp(
      syncer::TimeToProtoTime(base::Time::Now()));
  specifics.mutable_invalidation_fields()->set_instance_id_token(
      fcm_registration_token);
  sync_pb::InvalidationSpecificFields* mutable_invalidation_fields =
      specifics.mutable_invalidation_fields();
  for (DataType type : interested_data_types) {
    mutable_invalidation_fields->add_interested_data_type_ids(
        syncer::GetSpecificsFieldNumberFromDataType(type));
  }
  return specifics;
}

class SingleClientSyncInvalidationsTest : public SyncTest {
 public:
  SingleClientSyncInvalidationsTest() : SyncTest(SINGLE_CLIENT) {
  }

  // Injects a test DeviceInfo entity to the fake server.
  void InjectDeviceInfoEntityToServer(
      const std::string& cache_guid,
      syncer::DataTypeSet interested_data_types,
      const std::string& fcm_registration_token) {
    sync_pb::EntitySpecifics specifics;
    *specifics.mutable_device_info() = CreateDeviceInfoSpecifics(
        cache_guid, interested_data_types, fcm_registration_token);
    GetFakeServer()->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/"",
            /*client_tag=*/
            syncer::DeviceInfoUtil::SpecificsToTag(specifics.device_info()),
            specifics,
            /*creation_time=*/specifics.device_info().last_updated_timestamp(),
            /*last_modified_time=*/
            specifics.device_info().last_updated_timestamp()));
  }

  std::string GetLocalCacheGuid() {
    syncer::SyncTransportDataPrefs prefs(
        GetProfile(0)->GetPrefs(),
        GetClient(0)->GetGaiaIdHashForPrimaryAccount());
    return prefs.GetCacheGuid();
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientSyncInvalidationsTest,
                       SendInterestedDataTypesAndFCMTokenAsPartOfDeviceInfo) {
  ASSERT_TRUE(SetupSync());

  syncer::SyncInvalidationsService* sync_invalidations_service =
      SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0));
  ASSERT_THAT(sync_invalidations_service, NotNull());
  ASSERT_TRUE(sync_invalidations_service->GetInterestedDataTypes());
  ASSERT_TRUE(sync_invalidations_service->GetFCMRegistrationToken());
  const syncer::DataTypeSet interested_data_types =
      *sync_invalidations_service->GetInterestedDataTypes();
  const std::string fcm_token =
      *sync_invalidations_service->GetFCMRegistrationToken();

  // Check that some "standard" data types are included.
  EXPECT_TRUE(
      interested_data_types.HasAll({syncer::NIGORI, syncer::BOOKMARKS}));
  EXPECT_FALSE(fcm_token.empty());

  // The local device should eventually be committed to the server.
  EXPECT_TRUE(
      ServerDeviceInfoMatchChecker(
          ElementsAre(AllOf(InterestedDataTypesAre(interested_data_types),
                            HasInstanceIdToken(fcm_token))))
          .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientSyncInvalidationsTest,
                       ShouldPropagateInvalidationHints) {
  ASSERT_TRUE(SetupSync());

  // Simulate a server-side change which generates an invalidation.
  base::Uuid bookmark_uuid = InjectSyncedBookmark(GetFakeServer());
  ASSERT_TRUE(
      bookmarks_helper::BookmarksUuidChecker(/*profile=*/0, bookmark_uuid)
          .Wait());

  sync_pb::ClientToServerMessage message;
  ASSERT_TRUE(GetFakeServer()->GetLastGetUpdatesMessage(&message));

  // Verify that the latest GetUpdates happened due to an invalidation.
  ASSERT_EQ(message.get_updates().get_updates_origin(),
            sync_pb::SyncEnums::GU_TRIGGER);

  // Find progress marker for BOOKMARKS.
  sync_pb::DataTypeProgressMarker bookmark_progress_marker;
  for (const sync_pb::DataTypeProgressMarker& progress_marker :
       message.get_updates().from_progress_marker()) {
    if (progress_marker.data_type_id() ==
        GetSpecificsFieldNumberFromDataType(syncer::BOOKMARKS)) {
      bookmark_progress_marker = progress_marker;
    } else {
      // Other progress markers shouldn't contain hints.
      EXPECT_THAT(progress_marker.get_update_triggers().notification_hint(),
                  IsEmpty());
    }
  }

  // Verify that BOOKMARKS progress marker was found and contains a non-empty
  // notification hint.
  ASSERT_TRUE(bookmark_progress_marker.has_data_type_id());
  EXPECT_THAT(
      bookmark_progress_marker.get_update_triggers().notification_hint(),
      Contains(Not(IsEmpty())));
}

IN_PROC_BROWSER_TEST_F(SingleClientSyncInvalidationsTest,
                       ShouldPopulateFCMRegistrationTokens) {
  const std::string kTitle = "title";
  const std::string kRemoteDeviceCacheGuid = "other_cache_guid";
  const std::string kRemoteFCMRegistrationToken = "other_fcm_token";

  // Simulate the case when the server already knows another device which is
  // subscribed to all data types.
  InjectDeviceInfoEntityToServer(kRemoteDeviceCacheGuid,
                                 DefaultInterestedDataTypes(),
                                 kRemoteFCMRegistrationToken);
  ASSERT_TRUE(SetupSync());

  // Commit a new bookmark to check if the next commit message has FCM
  // registration tokens.
  AddFolder(0, GetBookmarkBarNode(0), 0, kTitle);
  ASSERT_TRUE(ServerBookmarksEqualityChecker({{kTitle, GURL()}},
                                             /*cryptographer=*/nullptr)
                  .Wait());

  sync_pb::ClientToServerMessage message;
  GetFakeServer()->GetLastCommitMessage(&message);

  EXPECT_THAT(
      message.commit().config_params().devices_fcm_registration_tokens(),
      ElementsAre(kRemoteFCMRegistrationToken));
  EXPECT_THAT(message.commit()
                  .config_params()
                  .fcm_registration_tokens_for_interested_clients(),
              ElementsAre(kRemoteFCMRegistrationToken));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientSyncInvalidationsTest,
    ShouldNotPopulateFCMRegistrationTokensForInterestedDataTypes) {
  const std::string kTitle = "title";
  const std::string kRemoteDeviceCacheGuid = "other_cache_guid";
  const std::string kRemoteFCMRegistrationToken = "other_fcm_token";

  // Simulate the case when the server already knows another device which is
  // not subscribed to BOOKMARKS.
  InjectDeviceInfoEntityToServer(
      kRemoteDeviceCacheGuid,
      Difference(DefaultInterestedDataTypes(), {syncer::BOOKMARKS}),
      kRemoteFCMRegistrationToken);
  ASSERT_TRUE(SetupSync());

  // Commit a new bookmark to check if the next commit message has FCM
  // registration tokens.
  AddFolder(0, GetBookmarkBarNode(0), 0, kTitle);
  ASSERT_TRUE(ServerBookmarksEqualityChecker({{kTitle, GURL()}},
                                             /*cryptographer=*/nullptr)
                  .Wait());

  sync_pb::ClientToServerMessage message;
  GetFakeServer()->GetLastCommitMessage(&message);

  // |devices_fcm_registration_tokens| still contains remote FCM registration
  // token because it's set regardless interested data type list.
  EXPECT_THAT(
      message.commit().config_params().devices_fcm_registration_tokens(),
      ElementsAre(kRemoteFCMRegistrationToken));
  EXPECT_THAT(message.commit()
                  .config_params()
                  .fcm_registration_tokens_for_interested_clients(),
              IsEmpty());
}

IN_PROC_BROWSER_TEST_F(SingleClientSyncInvalidationsTest,
                       ShouldProvideNotificationsEnabledInGetUpdates) {
  ASSERT_TRUE(SetupSync());

  // Trigger a new sync cycle by a server-side change to initiate GU_TRIGGER
  // GetUpdates.
  base::Uuid bookmark_uuid = InjectSyncedBookmark(GetFakeServer());
  ASSERT_TRUE(
      bookmarks_helper::BookmarksUuidChecker(/*profile=*/0, bookmark_uuid)
          .Wait());

  sync_pb::ClientToServerMessage message;
  ASSERT_TRUE(GetFakeServer()->GetLastGetUpdatesMessage(&message));

  // Verify that the latest GetUpdates happened due to an invalidation.
  ASSERT_EQ(message.get_updates().get_updates_origin(),
            sync_pb::SyncEnums::GU_TRIGGER);
  EXPECT_TRUE(message.get_updates().caller_info().notifications_enabled());

  ASSERT_GT(message.get_updates().from_progress_marker_size(), 0);
  ASSERT_TRUE(
      message.get_updates().from_progress_marker(0).has_get_update_triggers());
  ASSERT_TRUE(message.get_updates()
                  .from_progress_marker(0)
                  .get_update_triggers()
                  .has_invalidations_out_of_sync());
  EXPECT_FALSE(message.get_updates()
                   .from_progress_marker(0)
                   .get_update_triggers()
                   .invalidations_out_of_sync());
}

// PRE_* tests aren't supported on Android browser tests.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SingleClientSyncInvalidationsTest,
                       PRE_ShouldNotSendAdditionalGetUpdates) {
  ASSERT_TRUE(SetupSync());
}

IN_PROC_BROWSER_TEST_F(SingleClientSyncInvalidationsTest,
                       ShouldNotSendAdditionalGetUpdates) {
  const std::vector<sync_pb::SyncEntity> server_device_infos_before =
      fake_server_->GetSyncEntitiesByDataType(syncer::DEVICE_INFO);

  // Check here for size only, cache UUID will be verified after SetupcClients()
  // call.
  ASSERT_THAT(server_device_infos_before, SizeIs(1));
  const int64_t last_updated_timestamp = server_device_infos_before.front()
                                             .specifics()
                                             .device_info()
                                             .last_updated_timestamp();

  GetUpdatesTriggeredObserver observer(GetFakeServer(), DataType::AUTOFILL);
  ASSERT_TRUE(SetupClients());
  ASSERT_THAT(server_device_infos_before,
              ElementsAre(HasCacheGuid(GetLocalCacheGuid())));

  // Trigger DeviceInfo reupload once it has been initialized. This is mimics
  // the case when DeviceInfo is outdated on browser startup.
  DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(0))
      ->GetDeviceInfoTracker()
      ->ForcePulseForTest();
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());

  // Wait until DeviceInfo is updated.
  ASSERT_TRUE(ServerDeviceInfoMatchChecker(
                  ElementsAre(HasBeenUpdatedAfter(last_updated_timestamp)))
                  .Wait());

  // Perform an additional sync cycle to be sure that there will be at least one
  // more GetUpdates request if it was triggered.
  const std::string kTitle1 = "Title 1";
  AddFolder(0, GetBookmarkBarNode(0), 0, kTitle1);
  ASSERT_TRUE(ServerBookmarksEqualityChecker({{kTitle1, GURL()}},
                                             /*cryptographer=*/nullptr)
                  .Wait());

  const std::string kTitle2 = "Title 2";
  AddFolder(0, GetBookmarkBarNode(0), 0, kTitle2);
  ASSERT_TRUE(
      ServerBookmarksEqualityChecker({{kTitle1, GURL()}, {kTitle2, GURL()}},
                                     /*cryptographer=*/nullptr)
          .Wait());

  EXPECT_EQ(0u, observer.num_nudged_get_updates_for_data_type());
}

IN_PROC_BROWSER_TEST_F(SingleClientSyncInvalidationsTest,
                       PRE_ShouldReceiveInvalidationSentBeforeSetupClients) {
  // Initialize and enable sync to simulate browser restart when sync is
  // enabled. This is required to receive an invalidation when browser is not
  // loaded.
  ASSERT_TRUE(SetupSync());
}

IN_PROC_BROWSER_TEST_F(SingleClientSyncInvalidationsTest,
                       ShouldReceiveInvalidationSentBeforeSetupClients) {
  const base::Uuid bookmark_uuid = InjectSyncedBookmark(GetFakeServer());

  ASSERT_TRUE(SetupClients());

  // When configuration refresher is disabled, the following condition will be
  // possible only if invalidations are delivered.
  EXPECT_TRUE(
      bookmarks_helper::BookmarksUuidChecker(/*profile=*/0, bookmark_uuid)
          .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientSyncInvalidationsTest,
                       PRE_PersistBookmarkInvalidation) {
  ASSERT_TRUE(SetupSync());

  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);

  // Simulate a server-side change which generates an invalidation.
  InjectSyncedBookmark(GetFakeServer());
  ASSERT_TRUE(GetUpdatesFailureChecker(GetSyncService(0)).Wait());

  // Verify that the invalidation was used during the last sync cycle, but since
  // the GetUpdates request was not successful, the invalidation should still be
  // persisted on the client
  sync_pb::ClientToServerMessage last_get_updates;
  ASSERT_TRUE(GetFakeServer()->GetLastGetUpdatesMessage(&last_get_updates));
  sync_pb::DataTypeProgressMarker progress_marker = GetProgressMarkerForType(
      last_get_updates.get_updates(), syncer::BOOKMARKS);
  ASSERT_THAT(progress_marker.get_update_triggers().notification_hint(),
              Not(IsEmpty()));
}

IN_PROC_BROWSER_TEST_F(SingleClientSyncInvalidationsTest,
                       PersistBookmarkInvalidation) {
  ASSERT_TRUE(SetupClients()) << "SetupClient() failed.";
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());

  // TODO(crbug.com/40239360): Persisted invaldiations are loaded in
  // DataTypeWorker::ctor(), but sync cycle is not scheduled. New sync cycle
  // has to be triggered right after we loaded persisted invalidations.
  GetSyncService(0)->TriggerRefresh({syncer::BOOKMARKS});
  EXPECT_TRUE(NotificationHintChecker(syncer::BOOKMARKS).Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientSyncInvalidationsTest,
                       PRE_PersistDeviceInfoInvalidation) {
  const std::string kRemoteDeviceCacheGuid = "other_cache_guid";
  const std::string kRemoteFCMRegistrationToken = "other_fcm_token";
  ASSERT_TRUE(SetupSync());

  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);

  // Simulate a server-side change which generates an invalidation.
  InjectDeviceInfoEntityToServer(kRemoteDeviceCacheGuid,
                                 DefaultInterestedDataTypes(),
                                 kRemoteFCMRegistrationToken);
  ASSERT_TRUE(GetUpdatesFailureChecker(GetSyncService(0)).Wait());

  // Verify that the invalidation was used during the last sync cycle, but since
  // the GetUpdates request was not successful, the invalidation should still be
  // persisted on the client.
  sync_pb::ClientToServerMessage last_get_updates;
  ASSERT_TRUE(GetFakeServer()->GetLastGetUpdatesMessage(&last_get_updates));
  sync_pb::DataTypeProgressMarker progress_marker = GetProgressMarkerForType(
      last_get_updates.get_updates(), syncer::DEVICE_INFO);
  ASSERT_THAT(progress_marker.get_update_triggers().notification_hint(),
              Not(IsEmpty()));
}

IN_PROC_BROWSER_TEST_F(SingleClientSyncInvalidationsTest,
                       PersistDeviceInfoInvalidation) {
  ASSERT_TRUE(SetupClients()) << "SetupClient() failed.";
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());

  // TODO(crbug.com/40239360): Persisted invaldiations are loaded in
  // DataTypeWorker::ctor(), but sync cycle is not scheduled. New sync cycle
  // has to be triggered right after we loaded persisted invalidations.
  GetSyncService(0)->TriggerRefresh({syncer::DEVICE_INFO});
  EXPECT_TRUE(NotificationHintChecker(syncer::DEVICE_INFO).Wait());
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(SingleClientSyncInvalidationsTest,
                       EnableAndDisableADataType) {
  ASSERT_TRUE(SetupSync());

  // The local device should eventually be committed to the server. BOOKMARKS
  // should be included in interested types, since it's enabled by default.
  EXPECT_TRUE(ServerDeviceInfoMatchChecker(
                  ElementsAre(InterestedDataTypesContain(syncer::BOOKMARKS)))
                  .Wait());

  // Disable BOOKMARKS.
  ASSERT_TRUE(
      GetClient(0)->DisableSyncForType(syncer::UserSelectableType::kBookmarks));
  // The local device should eventually be committed to the server. BOOKMARKS
  // should not be included in interested types, as it was disabled.
  EXPECT_TRUE(
      ServerDeviceInfoMatchChecker(
          ElementsAre(Not(InterestedDataTypesContain(syncer::BOOKMARKS))))
          .Wait());

  // Create a bookmark on the server.
  InjectSyncedBookmark(GetFakeServer());
  // Enable BOOKMARKS again.
  ASSERT_TRUE(
      GetClient(0)->EnableSyncForType(syncer::UserSelectableType::kBookmarks));
  // The local device should eventually be committed to the server. BOOKMARKS
  // should now be included in interested types.
  EXPECT_TRUE(ServerDeviceInfoMatchChecker(
                  ElementsAre(InterestedDataTypesContain(syncer::BOOKMARKS)))
                  .Wait());
  // The bookmark should get synced now.
  EXPECT_TRUE(bookmarks_helper::GetBookmarkModel(0)->IsBookmarked(
      GURL(kSyncedBookmarkURL)));
}

// ChromeOS doesn't have the concept of sign-out.
#if !BUILDFLAG(IS_CHROMEOS_ASH)

// TODO(crbug.com/40833316): Enable test on Android once signout is supported.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SignoutAndSignin DISABLED_SignoutAndSignin
#else
#define MAYBE_SignoutAndSignin SignoutAndSignin
#endif
IN_PROC_BROWSER_TEST_F(SingleClientSyncInvalidationsTest,
                       MAYBE_SignoutAndSignin) {
  ASSERT_TRUE(SetupSync());

  // The local device should eventually be committed to the server. The FCM
  // token should be present in device info.
  ASSERT_TRUE(SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0))
                  ->GetFCMRegistrationToken());
  const std::string old_token =
      *SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0))
           ->GetFCMRegistrationToken();
  EXPECT_TRUE(
      ServerDeviceInfoMatchChecker(ElementsAre(HasInstanceIdToken(old_token)))
          .Wait());

  // Sign out. The FCM token should be cleared.
  GetClient(0)->SignOutPrimaryAccount();
  ASSERT_FALSE(SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0))
                   ->GetFCMRegistrationToken());

  // Sign in again.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_TRUE(GetClient(0)->AwaitInvalidationsStatus(/*expected_status=*/true));
  ASSERT_TRUE(SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0))
                  ->GetFCMRegistrationToken());
  const std::string new_token =
      *SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0))
           ->GetFCMRegistrationToken();
  EXPECT_NE(new_token, old_token);
  EXPECT_FALSE(new_token.empty());
  // The new device info (including the new FCM token) should eventually be
  // committed to the server.
  EXPECT_TRUE(
      ServerDeviceInfoMatchChecker(Contains(HasInstanceIdToken(new_token)))
          .Wait());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
