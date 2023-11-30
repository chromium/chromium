// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/enum_set.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/sync/account_bookmark_sync_service_factory.h"
#include "chrome/browser/sync/local_or_syncable_bookmark_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/committed_all_nudged_changes_checker.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/pref_names.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/core/dual_reading_list_model.h"
#include "components/reading_list/core/mock_reading_list_model_observer.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/test/fake_server.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using fake_server::FakeServer;
using sync_pb::SyncEnums;
using syncer::ModelType;
using syncer::ModelTypeSet;

namespace {

// Collects all the updated data types and used GetUpdates origins.
class GetUpdatesObserver : public FakeServer::Observer {
 public:
  explicit GetUpdatesObserver(FakeServer* fake_server)
      : fake_server_(fake_server) {
    DCHECK(fake_server);
    fake_server_->AddObserver(this);
  }

  ~GetUpdatesObserver() override { fake_server_->RemoveObserver(this); }

  using GetUpdatesOriginSet = base::EnumSet<SyncEnums::GetUpdatesOrigin,
                                            SyncEnums::GetUpdatesOrigin_MIN,
                                            SyncEnums::GetUpdatesOrigin_MAX>;

  // fake_server::FakeServer::Observer overrides.
  void OnSuccessfulGetUpdates() override {
    sync_pb::ClientToServerMessage message;
    fake_server_->GetLastGetUpdatesMessage(&message);
    DCHECK_NE(message.get_updates().get_updates_origin(),
              SyncEnums::UNKNOWN_ORIGIN);

    get_updates_origins_.Put(message.get_updates().get_updates_origin());
    for (const sync_pb::DataTypeProgressMarker& progress_marker :
         message.get_updates().from_progress_marker()) {
      ModelType type = syncer::GetModelTypeFromSpecificsFieldNumber(
          progress_marker.data_type_id());
      DCHECK_NE(type, ModelType::UNSPECIFIED);
      updated_types_.Put(type);
    }
  }

  GetUpdatesOriginSet GetAllOrigins() const { return get_updates_origins_; }

  ModelTypeSet GetUpdatedTypes() const { return updated_types_; }

 private:
  const raw_ptr<FakeServer> fake_server_;

  GetUpdatesOriginSet get_updates_origins_;
  ModelTypeSet updated_types_;
};

class SingleClientCommonSyncTest : public SyncTest {
 public:
  SingleClientCommonSyncTest() : SyncTest(SINGLE_CLIENT) {
    override_features_.InitWithFeatures(
        /*enabled_features=*/
        {password_manager::features::kPasswordManagerEnableReceiverService,
         password_manager::features::kPasswordManagerEnableSenderService},
        /*disabled_features=*/{});
  }
  ~SingleClientCommonSyncTest() override = default;
  SingleClientCommonSyncTest(const SingleClientCommonSyncTest&) = delete;
  SingleClientCommonSyncTest& operator=(const SingleClientCommonSyncTest&) =
      delete;

 private:
  base::test::ScopedFeatureList override_features_;
};

// Android doesn't currently support PRE_ tests, see crbug.com/1117345.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SingleClientCommonSyncTest,
                       PRE_ShouldNotIssueGetUpdatesOnBrowserRestart) {
  ASSERT_TRUE(SetupSync());
}

IN_PROC_BROWSER_TEST_F(SingleClientCommonSyncTest,
                       ShouldNotIssueGetUpdatesOnBrowserRestart) {
  GetUpdatesObserver get_updates_observer(GetFakeServer());

  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());

  // Some data types may use preconditions in the model type controller to
  // postpone their startup. Since such data types were paused (even for a short
  // period), an additional GetUpdates request may be sent during initialization
  // for them.
  // TODO(crbug.com/1432855): remove once GetUpdates is not issued anymore.
  GetUpdatesObserver::GetUpdatesOriginSet get_updates_origins_to_exclude{
      SyncEnums::PROGRAMMATIC};
  ModelTypeSet types_to_exclude{ModelType::ARC_PACKAGE, ModelType::HISTORY,
                                ModelType::CONTACT_INFO, ModelType::NIGORI};

  // Verify that there were no unexpected GetUpdates requests during Sync
  // initialization.
  // TODO(crbug.com/1418329): wait for invalidations to initialize and consider
  // making a Commit request. This would help to verify that there are no
  // unnecessary GetUpdates requests after browser restart.
  EXPECT_TRUE(Difference(get_updates_observer.GetAllOrigins(),
                         get_updates_origins_to_exclude)
                  .Empty());
  EXPECT_TRUE(
      Difference(get_updates_observer.GetUpdatedTypes(), types_to_exclude)
          .Empty())
      << "Updated data types: " << get_updates_observer.GetUpdatedTypes();
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/1465272): Deflake and reenable the test.
#define MAYBE_ShouldGetTypesWithUnsyncedDataFromSyncService \
  DISABLED_ShouldGetTypesWithUnsyncedDataFromSyncService
#else
#define MAYBE_ShouldGetTypesWithUnsyncedDataFromSyncService \
  ShouldGetTypesWithUnsyncedDataFromSyncService
#endif
IN_PROC_BROWSER_TEST_F(SingleClientCommonSyncTest,
                       MAYBE_ShouldGetTypesWithUnsyncedDataFromSyncService) {
  const std::string kBookmarkFolderTitle = "title1";
  const syncer::ModelTypeSet kInterestingDataTypes{syncer::BOOKMARKS,
                                                   syncer::PREFERENCES};

  ASSERT_TRUE(SetupClients());

  // Set the preference to false initially which should get synced.
  GetProfile(0)->GetPrefs()->SetBoolean(prefs::kHomePageIsNewTabPage, false);
  ASSERT_TRUE(SetupSync());
  absl::optional<sync_pb::PreferenceSpecifics> server_value =
      preferences_helper::GetPreferenceInFakeServer(
          syncer::ModelType::PREFERENCES, prefs::kHomePageIsNewTabPage,
          GetFakeServer());
  ASSERT_TRUE(server_value.has_value());
  ASSERT_EQ(server_value->value(), "false");

  {
    // No types have unsynced data.
    base::RunLoop loop;
    base::MockOnceCallback<void(syncer::ModelTypeSet)> callback;
    EXPECT_CALL(callback, Run(ModelTypeSet())).WillOnce([&]() { loop.Quit(); });
    GetSyncService(0)->GetTypesWithUnsyncedData(kInterestingDataTypes,
                                                callback.Get());
    loop.Run();
  }

  // Start throttling PREFERENCES so further commits will be rejected by the
  // server.
  GetFakeServer()->SetThrottledTypes({syncer::PREFERENCES});

  // Make local changes for PREFERENCES and BOOKMARKS, but the first is
  // throttled.
  GetProfile(0)->GetPrefs()->SetBoolean(prefs::kHomePageIsNewTabPage, true);
  bookmarks_helper::AddFolder(0, 0, kBookmarkFolderTitle);

  ASSERT_TRUE(AwaitQuiescence());

  // The bookmark should get committed successfully.
  ASSERT_TRUE(bookmarks_helper::ServerBookmarksEqualityChecker(
                  {{kBookmarkFolderTitle, GURL()}},
                  /*cryptographer=*/nullptr)
                  .Wait());

  // The preference should remain unsynced (still set to the previous value).
  ASSERT_EQ(preferences_helper::GetPreferenceInFakeServer(
                syncer::ModelType::PREFERENCES, prefs::kHomePageIsNewTabPage,
                GetFakeServer())
                ->value(),
            "false");

  {
    // PREFERENCES now has local changes not yet synced with the server.
    base::RunLoop loop;
    base::MockOnceCallback<void(syncer::ModelTypeSet)> callback;
    EXPECT_CALL(callback, Run(ModelTypeSet({syncer::PREFERENCES})))
        .WillOnce([&]() { loop.Quit(); });
    GetSyncService(0)->GetTypesWithUnsyncedData(kInterestingDataTypes,
                                                callback.Get());
    loop.Run();
  }

  // Unthrottle PREFERENCES to verify that sync can resume.
  GetFakeServer()->SetThrottledTypes(syncer::ModelTypeSet());

  // Wait for PREFERENCES to be de-throttled and commit local changes.
  ASSERT_TRUE(CommittedAllNudgedChangesChecker(GetSyncService(0)).Wait());
  ASSERT_EQ(preferences_helper::GetPreferenceInFakeServer(
                syncer::ModelType::PREFERENCES, prefs::kHomePageIsNewTabPage,
                GetFakeServer())
                ->value(),
            "true");

  {
    // No types have unsynced data.
    base::RunLoop loop;
    base::MockOnceCallback<void(syncer::ModelTypeSet)> callback;
    EXPECT_CALL(callback, Run(ModelTypeSet())).WillOnce([&]() { loop.Quit(); });
    GetSyncService(0)->GetTypesWithUnsyncedData(kInterestingDataTypes,
                                                callback.Get());
    loop.Run();
  }
}

// Android doesn't currently support PRE_ tests, see crbug.com/1117345.
#if !BUILDFLAG(IS_ANDROID)
void WaitForReadingListModelLoaded(ReadingListModel* reading_list_model) {
  testing::NiceMock<MockReadingListModelObserver> observer_;
  base::RunLoop run_loop;
  EXPECT_CALL(observer_, ReadingListModelLoaded).WillOnce([&run_loop] {
    run_loop.Quit();
  });
  reading_list_model->AddObserver(&observer_);
  run_loop.Run();
  reading_list_model->RemoveObserver(&observer_);
}

std::unique_ptr<syncer::LoopbackServerEntity> CreateTestReadingListEntity(
    const GURL& url,
    const std::string& entry_title) {
  sync_pb::EntitySpecifics specifics;
  *specifics.mutable_reading_list() = *base::MakeRefCounted<ReadingListEntry>(
                                           url, entry_title, base::Time::Now())
                                           ->AsReadingListSpecifics()
                                           .get();
  return syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
      "non_unique_name", url.spec(), specifics,
      /*creation_time=*/syncer::TimeToProtoTime(base::Time::Now()),
      /*last_modified_time=*/syncer::TimeToProtoTime(base::Time::Now()));
}

class SingleClientFeatureToTransportSyncTest : public SyncTest {
 public:
  SingleClientFeatureToTransportSyncTest() : SyncTest(SINGLE_CLIENT) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {syncer::kReadingListEnableSyncTransportModeUponSignIn},
        /*disabled_features=*/{});
  }

  ~SingleClientFeatureToTransportSyncTest() override = default;

  void BeforeSetupClient(int index,
                         const base::FilePath& profile_path) override {
    if (!content::IsPreTest()) {
      base::FilePath prefs_path = profile_path.AppendASCII("Preferences");
      std::string prefs_string;
      ASSERT_TRUE(base::ReadFileToString(prefs_path, &prefs_string));
      absl::optional<base::Value> prefs = base::JSONReader::Read(prefs_string);
      ASSERT_TRUE(prefs);
      ASSERT_TRUE(prefs->is_dict());
      prefs->GetDict().SetByDottedPath(prefs::kGoogleServicesConsentedToSync,
                                       base::Value(false));

      absl::optional<std::string> updated_prefs_string =
          base::WriteJson(*prefs);
      ASSERT_TRUE(updated_prefs_string);
      ASSERT_TRUE(base::WriteFile(prefs_path, *updated_prefs_string));
    }
  }

  bool SetupClients() override {
    if (!SyncTest::SetupClients()) {
      return false;
    }

    WaitForReadingListModelLoaded(reading_list_model());
    return true;
  }

  reading_list::DualReadingListModel* reading_list_model() {
    return static_cast<reading_list::DualReadingListModel*>(
        ReadingListModelFactory::GetForBrowserContext(GetProfile(0)));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

 protected:
  const GURL kUrl{"https://url.com/"};
};

IN_PROC_BROWSER_TEST_F(SingleClientFeatureToTransportSyncTest,
                       PRE_ShouldFixBadMetadata) {
  fake_server_->InjectEntity(CreateTestReadingListEntity(kUrl, "Title"));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());

  // The ReadingList entry is in the local-or-syncable model.
  ASSERT_EQ(reading_list_model()->size(), 1ul);
  ASSERT_EQ(reading_list_model()->GetStorageStateForURLForTesting(kUrl),
            reading_list::DualReadingListModel::StorageStateForTesting::
                kExistsInLocalOrSyncableModelOnly);
  // Note: Sync metadata now exists in the local-or-syncable model.

  // Similarly for bookmarks: Even though no actual bookmarks were downloaded,
  // the local-or-syncable model is now tracking metadata. (Depending on feature
  // flags, the account model may or may not exist.)
  ASSERT_TRUE(
      LocalOrSyncableBookmarkSyncServiceFactory::GetForProfile(GetProfile(0))
          ->IsTrackingMetadata());
}

IN_PROC_BROWSER_TEST_F(SingleClientFeatureToTransportSyncTest,
                       ShouldFixBadMetadata) {
  base::HistogramTester histograms;

  ASSERT_TRUE(SetupClients());
  // BeforeSetupClient() in the fixture has mangled the prefs so that
  // Sync-the-feature is *not* active anymore, and Sync will start up in
  // transport mode instead.
  // Note that this means the persisted metadata is now in an inconsistent
  // state: There is persisted metadata for Sync-the-feature mode, even though
  // Sync is not actually in that mode anymore.
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  // Sync re-downloaded the ReadingList entry into the account store, so it now
  // exists in both.
  ASSERT_EQ(reading_list_model()->size(), 1ul);
  ASSERT_EQ(reading_list_model()->GetStorageStateForURLForTesting(kUrl),
            reading_list::DualReadingListModel::StorageStateForTesting::
                kExistsInBothModels);
  // Verify that the URL is marked as needing upload. Most importantly, this
  // call would CHECK-crash if both models were tracking metadata, so this
  // serves as verification that the Sync-the-feature mode metadata was cleaned
  // up.
  const base::flat_set<GURL> urls_to_upload =
      reading_list_model()->GetKeysThatNeedUploadToSyncServer();
  EXPECT_FALSE(urls_to_upload.empty());

  // Similarly for bookmarks: The local-or-syncable model's metadata should have
  // been cleared. (The account model may or may not be active and tracking
  // metadata now, depending on feature flags.)
  EXPECT_FALSE(
      LocalOrSyncableBookmarkSyncServiceFactory::GetForProfile(GetProfile(0))
          ->IsTrackingMetadata());

  // Generally: For data types that use two separate models, the metadata (of
  // the Sync-the-feature model) should have been cleared.
  histograms.ExpectBucketCount(
      "Sync.ClearMetadataWhileStopped",
      syncer::ModelTypeHistogramValue(syncer::BOOKMARKS), 1);
  histograms.ExpectBucketCount(
      "Sync.ClearMetadataWhileStopped",
      syncer::ModelTypeHistogramValue(syncer::PASSWORDS), 1);
  histograms.ExpectBucketCount(
      "Sync.ClearMetadataWhileStopped",
      syncer::ModelTypeHistogramValue(syncer::READING_LIST), 1);
  histograms.ExpectBucketCount(
      "Sync.ClearMetadataWhileStopped",
      syncer::ModelTypeHistogramValue(syncer::AUTOFILL_WALLET_DATA), 1);
  histograms.ExpectBucketCount(
      "Sync.ClearMetadataWhileStopped",
      syncer::ModelTypeHistogramValue(syncer::SEARCH_ENGINES), 1);

  // But for data types that use a single model in both transport mode and
  // Sync-the-feature mode (and that support transport mode in the first place),
  // the metadata should *not* have been cleared.
  histograms.ExpectBucketCount(
      "Sync.ClearMetadataWhileStopped",
      syncer::ModelTypeHistogramValue(syncer::DEVICE_INFO), 0);
  histograms.ExpectBucketCount(
      "Sync.ClearMetadataWhileStopped",
      syncer::ModelTypeHistogramValue(syncer::SHARING_MESSAGE), 0);
  histograms.ExpectBucketCount(
      "Sync.ClearMetadataWhileStopped",
      syncer::ModelTypeHistogramValue(syncer::SECURITY_EVENTS), 0);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
