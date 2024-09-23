// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/device_info_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/test/fake_server.h"
#include "components/sync/test/fake_server_http_post_provider.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/device_info_util.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using device_info_helper::HasCacheGuid;
using device_info_helper::HasSharingFields;
using syncer::DataType;
using syncer::DataTypeSet;
using testing::AllOf;
using testing::Contains;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::IsSupersetOf;
using testing::Not;
using testing::UnorderedElementsAre;

MATCHER(HasFullHardwareClass, "") {
  return !arg.specifics().device_info().full_hardware_class().empty();
}

MATCHER(IsFullHardwareClassEmpty, "") {
  return arg.specifics().device_info().full_hardware_class().empty();
}

MATCHER_P(ModelEntryHasCacheGuid, expected_cache_guid, "") {
  return arg->guid() == expected_cache_guid;
}

MATCHER_P(HasInterestedDataType, expected_data_type, "") {
  for (int32_t interested_data_type_id : arg.specifics()
                                             .device_info()
                                             .invalidation_fields()
                                             .interested_data_type_ids()) {
    if (interested_data_type_id ==
        syncer::GetSpecificsFieldNumberFromDataType(expected_data_type)) {
      return true;
    }
  }
  return false;
}

std::string CacheGuidForSuffix(int suffix) {
  return base::StringPrintf("cache guid %d", suffix);
}

std::string ClientNameForSuffix(int suffix) {
  return base::StringPrintf("client name %d", suffix);
}

std::string SyncUserAgentForSuffix(int suffix) {
  return base::StringPrintf("sync user agent %d", suffix);
}

std::string ChromeVersionForSuffix(int suffix) {
  return base::StringPrintf("chrome version %d", suffix);
}

std::string SigninScopedDeviceIdForSuffix(int suffix) {
  return base::StringPrintf("signin scoped device id %d", suffix);
}

DataTypeSet DefaultInterestedDataTypes() {
  return Difference(syncer::ProtocolTypes(), syncer::CommitOnlyTypes());
}

sync_pb::DeviceInfoSpecifics CreateSpecifics(
    int suffix,
    const std::string& fcm_registration_token,
    const DataTypeSet& interested_data_types) {
  sync_pb::DeviceInfoSpecifics specifics;
  specifics.set_cache_guid(CacheGuidForSuffix(suffix));
  specifics.set_client_name(ClientNameForSuffix(suffix));
  specifics.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_LINUX);
  specifics.set_sync_user_agent(SyncUserAgentForSuffix(suffix));
  specifics.set_chrome_version(ChromeVersionForSuffix(suffix));
  specifics.set_signin_scoped_device_id(SigninScopedDeviceIdForSuffix(suffix));
  specifics.set_last_updated_timestamp(
      syncer::TimeToProtoTime(base::Time::Now()));
  auto& mutable_interested_data_type_ids =
      *specifics.mutable_invalidation_fields()
           ->mutable_interested_data_type_ids();
  for (DataType type : interested_data_types) {
    mutable_interested_data_type_ids.Add(
        syncer::GetSpecificsFieldNumberFromDataType(type));
  }
  if (!fcm_registration_token.empty()) {
    specifics.mutable_invalidation_fields()->set_instance_id_token(
        fcm_registration_token);
  }
  return specifics;
}

// Creates specifics for a client without sync standalone invalidations.
sync_pb::DeviceInfoSpecifics CreateSpecifics(int suffix) {
  return CreateSpecifics(suffix, /*fcm_registration_token=*/"",
                         DefaultInterestedDataTypes());
}

// Waits for a DeviceInfo entity to be committed to the fake server (regardless
// whether the commit succeeds or not). Note that it doesn't handle disabled
// network case.
class DeviceInfoCommitChecker : public SingleClientStatusChangeChecker {
  // SingleClientStatusChangeChecker is used instead of
  // FakeServerMatchStatusChecker because current checker is used when there is
  // an HTTP error on the fake server.
 public:
  DeviceInfoCommitChecker(syncer::SyncServiceImpl* service,
                          fake_server::FakeServer* fake_server)
      : SingleClientStatusChangeChecker(service), fake_server_(fake_server) {}

  // StatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for DeviceInfo to be committed.";

    sync_pb::ClientToServerMessage message;
    fake_server_->GetLastCommitMessage(&message);
    for (const sync_pb::SyncEntity& entity : message.commit().entries()) {
      if (entity.specifics().has_device_info()) {
        return true;
      }
    }

    return false;
  }

 private:
  const raw_ptr<fake_server::FakeServer> fake_server_;
};

class SingleClientDeviceInfoSyncTest : public SyncTest {
 public:
  SingleClientDeviceInfoSyncTest() : SyncTest(SINGLE_CLIENT) {
    override_features_.InitWithFeatures(
        {syncer::kSkipInvalidationOptimizationsWhenDeviceInfoUpdated}, {});
  }

  SingleClientDeviceInfoSyncTest(const SingleClientDeviceInfoSyncTest&) =
      delete;
  SingleClientDeviceInfoSyncTest& operator=(
      const SingleClientDeviceInfoSyncTest&) = delete;

  ~SingleClientDeviceInfoSyncTest() override = default;

  std::string GetLocalCacheGuid() const {
    return GetCacheGuid(/*profile_index=*/0);
  }

  syncer::DeviceInfoTracker* GetDeviceInfoTracker() {
    return DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(0))
        ->GetDeviceInfoTracker();
  }

  // Injects a test DeviceInfo entity to the fake server with disabled sync
  // standalone invalidations, given |suffix|.
  void InjectDeviceInfoEntityToServer(int suffix) {
    InjectDeviceInfoSpecificsToServer(CreateSpecifics(suffix));
  }

  // Injects an arbitrary test DeviceInfo entity to the fake server.
  void InjectDeviceInfoSpecificsToServer(
      const sync_pb::DeviceInfoSpecifics& device_info_specifics) {
    sync_pb::EntitySpecifics specifics;
    *specifics.mutable_device_info() = device_info_specifics;
    GetFakeServer()->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/"",
            /*client_tag=*/
            syncer::DeviceInfoUtil::SpecificsToTag(specifics.device_info()),
            specifics,
            /*creation_time=*/0, /*last_modified_time=*/0));
  }

 private:
  base::test::ScopedFeatureList override_features_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       UmaEnabledSetFullHardwareClass) {
  bool uma_enabled = true;
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &uma_enabled);
  ASSERT_TRUE(SetupSync());

  EXPECT_THAT(fake_server_->GetSyncEntitiesByDataType(syncer::DEVICE_INFO),
              Contains(HasFullHardwareClass()));

  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       UmaDisabledFullHardwareClassEmpty) {
  bool uma_enabled = false;
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &uma_enabled);
  ASSERT_TRUE(SetupSync());

  EXPECT_THAT(fake_server_->GetSyncEntitiesByDataType(syncer::DEVICE_INFO),
              Contains(IsFullHardwareClassEmpty()));

  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}
#else
IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       UmaEnabledFullHardwareClassOnNonChromeOS) {
  bool uma_enabled = true;
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &uma_enabled);
  ASSERT_TRUE(SetupSync());

  EXPECT_THAT(fake_server_->GetSyncEntitiesByDataType(syncer::DEVICE_INFO),
              Contains(IsFullHardwareClassEmpty()));

  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest, CommitLocalDevice) {
  ASSERT_TRUE(SetupSync());

  // The local device should eventually be committed to the server.
  EXPECT_TRUE(ServerDeviceInfoMatchChecker(
                  ElementsAre(HasCacheGuid(GetLocalCacheGuid())))
                  .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest, DownloadRemoteDevices) {
  InjectDeviceInfoEntityToServer(/*suffix=*/1);
  InjectDeviceInfoEntityToServer(/*suffix=*/2);

  ASSERT_TRUE(SetupSync());

  // The local device may or may not already be committed at this point.
  ASSERT_THAT(fake_server_->GetSyncEntitiesByDataType(syncer::DEVICE_INFO),
              IsSupersetOf({HasCacheGuid(CacheGuidForSuffix(1)),
                            HasCacheGuid(CacheGuidForSuffix(2))}));

  EXPECT_THAT(
      GetDeviceInfoTracker()->GetAllDeviceInfo(),
      UnorderedElementsAre(ModelEntryHasCacheGuid(GetLocalCacheGuid()),
                           ModelEntryHasCacheGuid(CacheGuidForSuffix(1)),
                           ModelEntryHasCacheGuid(CacheGuidForSuffix(2))));
}

IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       DownloadRemoteDeviceWithoutChromeVersion) {
  sync_pb::DeviceInfoSpecifics device_info_specifics =
      CreateSpecifics(/*suffix=*/1);
  device_info_specifics.clear_chrome_version();
  InjectDeviceInfoSpecificsToServer(device_info_specifics);

  ASSERT_TRUE(SetupSync());

  // Devices without a chrome_version correspond to non-Chromium-based clients
  // and should be excluded.
  EXPECT_THAT(
      GetDeviceInfoTracker()->GetAllChromeDeviceInfo(),
      UnorderedElementsAre(ModelEntryHasCacheGuid(GetLocalCacheGuid())));
  EXPECT_THAT(
      GetDeviceInfoTracker()->GetAllDeviceInfo(),
      UnorderedElementsAre(ModelEntryHasCacheGuid(GetLocalCacheGuid()),
                           ModelEntryHasCacheGuid(CacheGuidForSuffix(1))));
}

IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       DownloadRemoteDeviceWithNewVersionFieldOnly) {
  sync_pb::DeviceInfoSpecifics device_info_specifics =
      CreateSpecifics(/*suffix=*/1);
  device_info_specifics.clear_chrome_version();
  device_info_specifics.mutable_chrome_version_info()->set_version_number(
      "someversion");
  InjectDeviceInfoSpecificsToServer(device_info_specifics);

  ASSERT_TRUE(SetupSync());

  // Devices without a chrome_version correspond to non-Chromium-based clients
  // and should be excluded.
  EXPECT_THAT(
      GetDeviceInfoTracker()->GetAllDeviceInfo(),
      UnorderedElementsAre(ModelEntryHasCacheGuid(GetLocalCacheGuid()),
                           ModelEntryHasCacheGuid(CacheGuidForSuffix(1))));
}

// On ChromeOS, Sync-the-feature gets started automatically once a primary
// account is signed in and transport mode is not a thing.
#if !BUILDFLAG(IS_CHROMEOS_ASH)

// TODO(crbug.com/40756482): Flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_CommitLocalDevice_TransportOnly \
  DISABLED_CommitLocalDevice_TransportOnly
#else
#define MAYBE_CommitLocalDevice_TransportOnly CommitLocalDevice_TransportOnly
#endif  // BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       MAYBE_CommitLocalDevice_TransportOnly) {
  ASSERT_TRUE(SetupClients());

  // Setup a primary account, but don't actually enable Sync-the-feature (so
  // that Sync will start in transport mode).
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::DEVICE_INFO));

  // The local device should eventually be committed to the server.
  EXPECT_TRUE(ServerDeviceInfoMatchChecker(
                  ElementsAre(HasCacheGuid(GetLocalCacheGuid())))
                  .Wait());
}

// TODO(crbug.com/40756482): Flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_DownloadRemoteDevices_TransportOnly \
  DISABLED_DownloadRemoteDevices_TransportOnly
#else
#define MAYBE_DownloadRemoteDevices_TransportOnly \
  DownloadRemoteDevices_TransportOnly
#endif  // BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       MAYBE_DownloadRemoteDevices_TransportOnly) {
  InjectDeviceInfoEntityToServer(/*suffix=*/1);
  InjectDeviceInfoEntityToServer(/*suffix=*/2);

  ASSERT_TRUE(SetupClients());

  // Setup a primary account, but don't actually enable Sync-the-feature (so
  // that Sync will start in transport mode).
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::DEVICE_INFO));

  EXPECT_THAT(fake_server_->GetSyncEntitiesByDataType(syncer::DEVICE_INFO),
              IsSupersetOf({HasCacheGuid(CacheGuidForSuffix(1)),
                            HasCacheGuid(CacheGuidForSuffix(2))}));
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       ShouldSetTheOnlyClientFlag) {
  ASSERT_TRUE(SetupSync());

  const std::vector<sync_pb::SyncEntity> entities_before =
      fake_server_->GetSyncEntitiesByDataType(syncer::DEVICE_INFO);

  // Single client flag could be dropped due to a DeviceInfo update in the last
  // GetUpdates request. The next sync cycle may download the latest committed
  // DeviceInfo reflection and drop optimization flags. Hence, make it sure that
  // there are at least 2 sync cycles and check the second one only.
  bookmarks_helper::AddURL(/*profile=*/0, "Title", GURL("http://foo.com"));
  ASSERT_TRUE(bookmarks_helper::BookmarkModelMatchesFakeServerChecker(
                  /*profile=*/0, GetSyncService(0), GetFakeServer())
                  .Wait());

  // Perform the second sync cycle.
  bookmarks_helper::AddURL(/*profile=*/0, "Title", GURL("http://foo.com"));
  ASSERT_TRUE(bookmarks_helper::BookmarkModelMatchesFakeServerChecker(
                  /*profile=*/0, GetSyncService(0), GetFakeServer())
                  .Wait());

  // Double check that DeviceInfo hasn't been committed during the test. It may
  // happen if there are any DeviceInfo fields are initialized asynchronously.
  const std::vector<sync_pb::SyncEntity> entities_after =
      fake_server_->GetSyncEntitiesByDataType(syncer::DEVICE_INFO);
  ASSERT_EQ(1U, entities_before.size());
  ASSERT_EQ(1U, entities_after.size());
  ASSERT_EQ(entities_before.front().mtime(), entities_after.front().mtime());

  sync_pb::ClientToServerMessage message;
  GetFakeServer()->GetLastCommitMessage(&message);

  EXPECT_TRUE(message.commit().config_params().single_client());
  EXPECT_TRUE(message.commit()
                  .config_params()
                  .single_client_with_standalone_invalidations());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientDeviceInfoSyncTest,
    ShouldSetTheOnlyClientFlagForStandaloneInvalidationsOnly) {
  // A client without standalone invalidations shouldn't affect |single_client|
  // flag.
  InjectDeviceInfoEntityToServer(/*suffix=*/1);

  ASSERT_TRUE(SetupSync());

  // Single client flag could be dropped due to a DeviceInfo update in the last
  // GetUpdates request. The next sync cycle may download the latest committed
  // DeviceInfo reflection and drop optimization flags. Hence, make it sure that
  // there are at least 2 sync cycles and check the second one only.
  bookmarks_helper::AddURL(/*profile=*/0, "Title", GURL("http://foo.com"));
  ASSERT_TRUE(bookmarks_helper::BookmarkModelMatchesFakeServerChecker(
                  /*profile=*/0, GetSyncService(0), GetFakeServer())
                  .Wait());

  // Perform the second sync cycle.
  bookmarks_helper::AddURL(/*profile=*/0, "Title", GURL("http://foo.com"));
  ASSERT_TRUE(bookmarks_helper::BookmarkModelMatchesFakeServerChecker(
                  /*profile=*/0, GetSyncService(0), GetFakeServer())
                  .Wait());

  sync_pb::ClientToServerMessage message;
  GetFakeServer()->GetLastCommitMessage(&message);

  EXPECT_FALSE(message.commit().config_params().single_client());
  EXPECT_TRUE(message.commit()
                  .config_params()
                  .single_client_with_standalone_invalidations());
}

IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       ShouldSetTheOnlyClientFlagForDataType) {
  // There is a remote client which is not interested in BOOKMARKS.
  const DataTypeSet remote_interested_data_types =
      Difference(DefaultInterestedDataTypes(), {syncer::BOOKMARKS});
  InjectDeviceInfoSpecificsToServer(CreateSpecifics(
      /*suffix=*/1, "fcm_token_1", remote_interested_data_types));

  ASSERT_TRUE(SetupSync());

  // Single client flag could be dropped due to a DeviceInfo update in the last
  // GetUpdates request. The next sync cycle may download the latest committed
  // DeviceInfo reflection and drop optimization flags. Hence, make it sure that
  // there are at least 2 sync cycles and check the second one only.
  bookmarks_helper::AddURL(/*profile=*/0, "Title", GURL("http://foo.com"));
  ASSERT_TRUE(bookmarks_helper::BookmarkModelMatchesFakeServerChecker(
                  /*profile=*/0, GetSyncService(0), GetFakeServer())
                  .Wait());

  // Perform the second sync cycle.
  bookmarks_helper::AddURL(/*profile=*/0, "Title", GURL("http://foo.com"));
  ASSERT_TRUE(bookmarks_helper::BookmarkModelMatchesFakeServerChecker(
                  /*profile=*/0, GetSyncService(0), GetFakeServer())
                  .Wait());

  sync_pb::ClientToServerMessage message;
  GetFakeServer()->GetLastCommitMessage(&message);

  // Even though there is another active client, that client is not interested
  // in the just-committed bookmark, so for the purpose of this commit, the
  // committing client is the "single" one.
  EXPECT_TRUE(message.commit().config_params().single_client());
  EXPECT_TRUE(message.commit()
                  .config_params()
                  .single_client_with_standalone_invalidations());
}

IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       ShouldNotProvideTheOnlyClientFlag) {
  InjectDeviceInfoSpecificsToServer(CreateSpecifics(
      /*suffix=*/1, "fcm_token_1", DefaultInterestedDataTypes()));

  ASSERT_TRUE(SetupSync());

  // Verify that both DeviceInfos are present on the server.
  ASSERT_THAT(GetFakeServer()->GetSyncEntitiesByDataType(syncer::DEVICE_INFO),
              UnorderedElementsAre(HasCacheGuid(GetLocalCacheGuid()),
                                   HasCacheGuid(CacheGuidForSuffix(1))));

  // Download all the updates from the server to prevent DeviceInfo update while
  // committing.
  GetSyncService(0)->TriggerRefresh({syncer::DEVICE_INFO});

  // Everything's ready to verify that the next commit request contains
  // single_client which is false. Commit a bookmark to trigger a commit
  // request.
  bookmarks_helper::AddURL(/*profile=*/0, "Title", GURL("http://foo.com"));
  ASSERT_TRUE(bookmarks_helper::BookmarkModelMatchesFakeServerChecker(
                  /*profile=*/0, GetSyncService(0), GetFakeServer())
                  .Wait());

  sync_pb::ClientToServerMessage message;
  GetFakeServer()->GetLastCommitMessage(&message);

  EXPECT_FALSE(message.commit().config_params().single_client());
  EXPECT_FALSE(message.commit()
                   .config_params()
                   .single_client_with_standalone_invalidations());
}

// This test verifies that single_client optimization flag is not set after
// DeviceInfo has been received (even within the same sync cycle).
IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       ShouldNotPopulateTheOnlyClientWhenDeviceInfoUpdated) {
  ASSERT_TRUE(SetupSync());

  const std::vector<sync_pb::SyncEntity> server_device_infos =
      GetFakeServer()->GetSyncEntitiesByDataType(syncer::DEVICE_INFO);
  ASSERT_THAT(server_device_infos,
              ElementsAre(HasCacheGuid(GetLocalCacheGuid())));

  // Simulate going offline to have both downloading and committing updates in
  // the same sync cycle.
  fake_server::FakeServerHttpPostProvider::DisableNetwork();

  // Add a DeviceInfo tombstone to cause a commit request (removing local
  // DeviceInfo will cause its reupload).
  GetFakeServer()->InjectEntity(
      syncer::PersistentTombstoneEntity::CreateFromEntity(
          server_device_infos.front()));

  // Simulate DeviceInfo update from a new remote client.
  InjectDeviceInfoEntityToServer(/*suffix=*/1);

  // Simulate going online. This starts a new sync cycle with both GetUpdates
  // and Commit requests.
  fake_server::FakeServerHttpPostProvider::EnableNetwork();

  // Waiting for a local DeviceInfo reupload.
  ASSERT_TRUE(ServerDeviceInfoMatchChecker(
                  UnorderedElementsAre(HasCacheGuid(GetLocalCacheGuid()),
                                       HasCacheGuid(CacheGuidForSuffix(1))))
                  .Wait());

  sync_pb::ClientToServerMessage message;
  GetFakeServer()->GetLastCommitMessage(&message);

  // Verify that all the optimization flags are omitted.
  EXPECT_FALSE(message.commit().config_params().single_client());
  EXPECT_FALSE(message.commit()
                   .config_params()
                   .single_client_with_standalone_invalidations());
  EXPECT_THAT(message.commit()
                  .config_params()
                  .fcm_registration_tokens_for_interested_clients(),
              IsEmpty());
}

IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       ShouldReuploadLocalDeviceIfRemovedFromServer) {
  ASSERT_TRUE(SetupSync());

  const std::vector<sync_pb::SyncEntity> server_device_infos =
      GetFakeServer()->GetSyncEntitiesByDataType(syncer::DEVICE_INFO);
  ASSERT_THAT(server_device_infos, Contains(HasCacheGuid(GetLocalCacheGuid())));

  GetFakeServer()->InjectEntity(
      syncer::PersistentTombstoneEntity::CreateFromEntity(
          server_device_infos.front()));

  // On receiving the tombstone, the client should reupload its own device info.
  EXPECT_TRUE(ServerDeviceInfoMatchChecker(
                  ElementsAre(HasCacheGuid(GetLocalCacheGuid())))
                  .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       ShouldRetryDeviceInfoCommitOnAuthError) {
  ASSERT_TRUE(SetupSync());

  GetFakeServer()->SetHttpError(net::HTTP_UNAUTHORIZED);

  // Disable another data type to trigger a commit of a new DeviceInfo entity.
  // Create a checker to catch a commit request before disabling the data type.
  DeviceInfoCommitChecker device_info_committer_checker(GetSyncService(0),
                                                        GetFakeServer());
  ASSERT_TRUE(
      GetClient(0)->DisableSyncForType(syncer::UserSelectableType::kBookmarks));
  ASSERT_TRUE(device_info_committer_checker.Wait());

  GetFakeServer()->ClearHttpError();

  // Wait for the DeviceInfo to be committed to the fake server again.
  EXPECT_TRUE(ServerDeviceInfoMatchChecker(
                  ElementsAre(Not(HasInterestedDataType(syncer::BOOKMARKS))))
                  .Wait());
}

// PRE_* tests aren't supported on Android browser tests.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       PRE_ShouldNotSendDeviceInfoAfterBrowserRestart) {
  ASSERT_TRUE(SetupSync());
}

IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       ShouldNotSendDeviceInfoAfterBrowserRestart) {
  const std::vector<sync_pb::SyncEntity> entities_before =
      fake_server_->GetSyncEntitiesByDataType(syncer::DEVICE_INFO);
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  ASSERT_TRUE(GetClient(0)->AwaitInvalidationsStatus(/*expected_status=*/true));

  bool has_local_changes = false;
  base::RunLoop run_loop;
  GetSyncService(0)->HasUnsyncedItemsForTest(
      base::BindLambdaForTesting([&has_local_changes, &run_loop](bool result) {
        has_local_changes = result;
        run_loop.Quit();
      }));
  run_loop.Run();

  const std::vector<sync_pb::SyncEntity> entities_after =
      fake_server_->GetSyncEntitiesByDataType(syncer::DEVICE_INFO);
  ASSERT_EQ(1U, entities_before.size());
  ASSERT_EQ(1U, entities_after.size());

  // Check that there are no local changes and nothing has been committed to the
  // server.
  EXPECT_FALSE(has_local_changes);
  EXPECT_EQ(entities_before.front().mtime(), entities_after.front().mtime());
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
