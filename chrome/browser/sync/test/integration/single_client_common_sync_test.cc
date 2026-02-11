// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/enum_set.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/account_bookmark_sync_service_factory.h"
#include "chrome/browser/sync/local_or_syncable_bookmark_sync_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/committed_all_nudged_changes_checker.h"
#include "chrome/browser/sync/test/integration/reading_list_helper.h"
#include "chrome/browser/sync/test/integration/send_tab_to_self_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/themes_helper.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/reading_list/core/dual_reading_list_model.h"
#include "components/reading_list/core/mock_reading_list_model_observer.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/cycle/entity_change_metric_recording.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/service/glue/sync_transport_data_prefs.h"
#include "components/sync/service/sync_service_utils.h"
#include "components/sync/test/fake_server.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using bookmarks_helper::GetBookmarkModel;
using fake_server::FakeServer;
using sync_pb::SyncEnums;
using syncer::DataType;
using syncer::DataTypeSet;
using testing::ElementsAre;

namespace {

#if !BUILDFLAG(IS_ANDROID)
std::unique_ptr<syncer::LoopbackServerEntity> CreateTombstone(
    syncer::DataType data_type,
    std::string_view client_tag) {
  const std::string client_tag_hash =
      syncer::ClientTagHash::FromUnhashed(data_type, client_tag).value();

  // For all data types except bookmarks, the server ID is built based on the
  // client tag *hash*. For bookmarks, the non-hashed client tag (aka UUID) is
  // used.
  return syncer::PersistentTombstoneEntity::CreateNew(
      syncer::LoopbackServerEntity::CreateId(
          data_type, (data_type == syncer::BOOKMARKS) ? std::string(client_tag)
                                                      : client_tag_hash),
      client_tag_hash);
}
#endif  // !BUILDFLAG(IS_ANDROID)

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
      DataType type = syncer::GetDataTypeFromSpecificsFieldNumber(
          progress_marker.data_type_id());
      DCHECK_NE(type, DataType::UNSPECIFIED);
      updated_types_.Put(type);
    }
  }

  GetUpdatesOriginSet GetAllOrigins() const { return get_updates_origins_; }

  DataTypeSet GetUpdatedTypes() const { return updated_types_; }

 private:
  const raw_ptr<FakeServer> fake_server_;

  GetUpdatesOriginSet get_updates_origins_;
  DataTypeSet updated_types_;
};

class SingleClientCommonSyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SingleClientCommonSyncTest() : SyncTest(SINGLE_CLIENT) {
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      scoped_feature_list_.InitAndEnableFeature(
          syncer::kReplaceSyncPromosWithSignInPromos);
    } else {
      // Skip sync-to-signin migration for sync-the-feature tests. This is to
      // avoid the sync state changing between the PRE_ tests.
      scoped_feature_list_.InitAndDisableFeature(
          switches::kMigrateSyncingUserToSignedIn);
    }
  }
  ~SingleClientCommonSyncTest() override = default;
  SingleClientCommonSyncTest(const SingleClientCommonSyncTest&) = delete;
  SingleClientCommonSyncTest& operator=(const SingleClientCommonSyncTest&) =
      delete;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SingleClientCommonSyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

// Android doesn't currently support PRE_ tests, see crbug.com/1117345.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(SingleClientCommonSyncTest,
                       PRE_ShouldNotIssueGetUpdatesOnBrowserRestart) {
  ASSERT_TRUE(SetupSync());
}

IN_PROC_BROWSER_TEST_P(SingleClientCommonSyncTest,
                       ShouldNotIssueGetUpdatesOnBrowserRestart) {
  GetUpdatesObserver get_updates_observer(GetFakeServer());

  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  // Some data types may use preconditions in the data type controller to
  // postpone their startup. Since such data types were paused (even for a short
  // period), an additional GetUpdates request may be sent during initialization
  // for them.
  // TODO(crbug.com/40264154): remove once GetUpdates is not issued anymore.
  GetUpdatesObserver::GetUpdatesOriginSet get_updates_origins_to_exclude{
      SyncEnums::PROGRAMMATIC};
  DataTypeSet types_to_exclude{
      DataType::ARC_PACKAGE, DataType::HISTORY, DataType::CONTACT_INFO,
      DataType::NIGORI,
      // TODO(crbug.com/410116020): Remove once these types pass this test.
      DataType::SHARED_TAB_GROUP_DATA, DataType::SHARED_TAB_GROUP_ACCOUNT_DATA,
      DataType::COLLABORATION_GROUP};

  // Verify that there were no unexpected GetUpdates requests during Sync
  // initialization.
  // TODO(crbug.com/40894668): wait for invalidations to initialize and consider
  // making a Commit request. This would help to verify that there are no
  // unnecessary GetUpdates requests after browser restart.
  EXPECT_TRUE(Difference(get_updates_observer.GetAllOrigins(),
                         get_updates_origins_to_exclude)
                  .empty());
  EXPECT_TRUE(
      Difference(get_updates_observer.GetUpdatedTypes(), types_to_exclude)
          .empty())
      << "Updated data types: " << get_updates_observer.GetUpdatedTypes();
}
#endif  // !BUILDFLAG(IS_ANDROID)

// ChromeOS doesn't support primary account signout.
#if !BUILDFLAG(IS_CHROMEOS)

// Note: See also SyncErrorTest.ClientDataObsoleteTest, which ensures the cache
// GUID does *not* get reused if the client's data needs to be reset.
IN_PROC_BROWSER_TEST_P(SingleClientCommonSyncTest,
                       ReusesCacheGuidAfterSignoutAndSignin) {
  ASSERT_TRUE(SetupSync());

  std::string cache_guid;
  {
    syncer::SyncTransportDataPrefs prefs(
        GetProfile(0)->GetPrefs(),
        GetClient(0)->GetGaiaIdHashForPrimaryAccount());
    cache_guid = prefs.GetCacheGuid();
  }
  ASSERT_FALSE(cache_guid.empty());

  GetClient(0)->SignOutPrimaryAccount();
  {
    // At this point there's no GaiaId, and thus no cache GUID either.
    syncer::SyncTransportDataPrefs prefs(
        GetProfile(0)->GetPrefs(),
        GetClient(0)->GetGaiaIdHashForPrimaryAccount());
    ASSERT_TRUE(prefs.GetCacheGuid().empty());
  }

  // When enabling Sync again, the cache GUID should get reused.
  ASSERT_TRUE(GetClient(0)->SetupSync());
  {
    syncer::SyncTransportDataPrefs prefs(
        GetProfile(0)->GetPrefs(),
        GetClient(0)->GetGaiaIdHashForPrimaryAccount());
    EXPECT_EQ(prefs.GetCacheGuid(), cache_guid);
  }
}

IN_PROC_BROWSER_TEST_P(SingleClientCommonSyncTest,
                       ReusesCacheGuidOnlyForSameAccount) {
  ASSERT_TRUE(SetupClients());

  ASSERT_TRUE(GetClient(0)->SetupSync(SyncTestAccount::kConsumerAccount1));

  std::string cache_guid1;
  {
    syncer::SyncTransportDataPrefs prefs(
        GetProfile(0)->GetPrefs(),
        GetClient(0)->GetGaiaIdHashForPrimaryAccount());
    cache_guid1 = prefs.GetCacheGuid();
  }
  ASSERT_FALSE(cache_guid1.empty());

  // Enable Sync with a different account.
  GetClient(0)->SignOutPrimaryAccount();
  ASSERT_TRUE(GetClient(0)->SetupSync(SyncTestAccount::kConsumerAccount2));

  std::string cache_guid2;
  {
    syncer::SyncTransportDataPrefs prefs(
        GetProfile(0)->GetPrefs(),
        GetClient(0)->GetGaiaIdHashForPrimaryAccount());
    cache_guid2 = prefs.GetCacheGuid();
  }
  ASSERT_FALSE(cache_guid2.empty());
  // The cache GUID should *not* be reused for the second account.
  EXPECT_NE(cache_guid1, cache_guid2);

  // Enable Sync with the first account again.
  GetClient(0)->SignOutPrimaryAccount();
  ASSERT_TRUE(GetClient(0)->SetupSync(SyncTestAccount::kConsumerAccount1));

  // The first cache GUID should have been reused.
  {
    syncer::SyncTransportDataPrefs prefs(
        GetProfile(0)->GetPrefs(),
        GetClient(0)->GetGaiaIdHashForPrimaryAccount());
    EXPECT_EQ(prefs.GetCacheGuid(), cache_guid1);
  }
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_P(SingleClientCommonSyncTest,
                       E2E_ENABLED(ShouldCrashAwaitQuiescenceForE2ETest)) {
  ASSERT_TRUE(SetupSync());
  EXPECT_CHECK_DEATH_WITH(
      { EXPECT_TRUE(AwaitQuiescence()); },
      "AwaitQuiescence is not supported for E2E tests.");
}

class SingleClientGetUnsyncedTypesTest : public SyncTest {
 public:
  SingleClientGetUnsyncedTypesTest() : SyncTest(SINGLE_CLIENT) {
#if !BUILDFLAG(IS_ANDROID)
    // These features are required to enable THEMES and BOOKMARK in transport
    // mode.
    feature_list_.InitWithFeatures(
        {switches::kEnablePreferencesAccountStorage,
         syncer::kSeparateLocalAndAccountThemes,
         switches::kSyncEnableBookmarksInTransportMode},
        {});
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  // Unsynced data is only valid with sync transport.
  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return SetupSyncMode::kSyncTransportOnly;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SingleClientGetUnsyncedTypesTest,
                       ShouldGetTypesWithUnsyncedDataFromSyncService) {
  ASSERT_TRUE(SetupSync());

#if !BUILDFLAG(IS_ANDROID)
  // Note: Depending on the state of feature flags (specifically
  // kReplaceSyncPromosWithSignInPromos), Bookmarks may or may not be considered
  // selected by default.
  GetSyncService(0)->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kBookmarks, true);
  // Enable account storage for bookmarks.
  SigninPrefs prefs(*GetProfile(0)->GetPrefs());
  const GaiaId gaia_id = GetSyncService(0)->GetSyncAccountInfoForPrefs().gaia;
  prefs.SetBookmarksExplicitBrowserSignin(gaia_id, true);
  ASSERT_TRUE(prefs.GetBookmarksExplicitBrowserSignin(gaia_id));
#endif  // !BUILDFLAG(IS_ANDROID)

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::BOOKMARKS));

  // BOOKMARKS has no unsynced data.
  EXPECT_FALSE(GetClient(0)
                   ->GetTypesWithUnsyncedDataAndWait({syncer::BOOKMARKS})
                   .contains(syncer::BOOKMARKS));

  ASSERT_TRUE(bookmarks_helper::BookmarkModelMatchesFakeServerChecker(
                  GetBookmarkModel(0), GetSyncService(0), GetFakeServer(),
                  bookmarks_helper::StoreType::kAccountStore)
                  .Wait());

  // Force bookmark saved to the account to be unsynced.
  GetFakeServer()->SetHttpError(net::HTTP_BAD_REQUEST);

  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile(0));
  model->AddURL(model->account_bookmark_bar_node(), 0, u"title1",
                GURL("https://example.com"));

  // BOOKMARKS now has local changes not yet synced with the server.
  EXPECT_TRUE(GetClient(0)
                  ->GetTypesWithUnsyncedDataAndWait({syncer::BOOKMARKS})
                  .contains(syncer::BOOKMARKS));

  // Clear the error and wait for the local changes to be committed.
  GetFakeServer()->ClearHttpError();
  ASSERT_TRUE(bookmarks_helper::BookmarkModelMatchesFakeServerChecker(
                  GetBookmarkModel(0), GetSyncService(0), GetFakeServer(),
                  bookmarks_helper::StoreType::kAccountStore)
                  .Wait());

  // BOOKMARKS has no unsynced data.
  EXPECT_FALSE(GetClient(0)
                   ->GetTypesWithUnsyncedDataAndWait({syncer::BOOKMARKS})
                   .contains(syncer::BOOKMARKS));
}

// The following test uses THEMES for testing, however, THEMES data type is not
// on Android.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SingleClientGetUnsyncedTypesTest, HttpError) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::THEMES));

  // THEMES has no unsynced data.
  ASSERT_FALSE(GetClient(0)
                   ->GetTypesWithUnsyncedDataAndWait({syncer::THEMES})
                   .contains(syncer::THEMES));

  // Force theme saved to the account to be unsynced.
  GetFakeServer()->SetHttpError(net::HTTP_BAD_REQUEST);

  // Set up a custom theme.
  themes_helper::UseCustomTheme(GetProfile(0), 0);
  ASSERT_TRUE(CustomThemeChecker(GetProfile(0)).Wait());

  // THEMES now has local changes not yet synced with the server.
  EXPECT_TRUE(GetClient(0)
                  ->GetTypesWithUnsyncedDataAndWait({syncer::THEMES})
                  .contains(syncer::THEMES));

  // Http error is not an auth error.
  EXPECT_FALSE(
      GetClient(0)->service()->HasCachedPersistentAuthErrorForMetrics());

  // Clear the error and wait for the local changes to be committed.
  GetFakeServer()->ClearHttpError();
  ASSERT_TRUE(CommittedAllNudgedChangesChecker(GetSyncService(0)).Wait());

  // THEMES has no unsynced data.
  EXPECT_FALSE(GetClient(0)
                   ->GetTypesWithUnsyncedDataAndWait({syncer::THEMES})
                   .contains(syncer::THEMES));
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Android currently doesn't support some methods used in this test, see
// crbug.com/40871747.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SingleClientGetUnsyncedTypesTest, SignInPendingState) {
  base::HistogramTester histograms;

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::THEMES));

  // THEMES has no unsynced data.
  ASSERT_FALSE(GetClient(0)
                   ->GetTypesWithUnsyncedDataAndWait({syncer::THEMES})
                   .contains(syncer::THEMES));

  // Enter sign-in pending state.
  ASSERT_TRUE(GetClient(0)->EnterSignInPendingStateForPrimaryAccount());

  // Set up a custom theme.
  themes_helper::UseCustomTheme(GetProfile(0), 0);
  ASSERT_TRUE(CustomThemeChecker(GetProfile(0)).Wait());

  // THEMES now has local changes not yet synced with the server.
  EXPECT_TRUE(GetClient(0)
                  ->GetTypesWithUnsyncedDataAndWait({syncer::THEMES})
                  .contains(syncer::THEMES));

  EXPECT_TRUE(
      GetClient(0)->service()->HasCachedPersistentAuthErrorForMetrics());

  // Clear the error and wait for the local changes to be committed.
  ASSERT_TRUE(GetClient(0)->ExitSignInPendingStateForPrimaryAccount());
  ASSERT_TRUE(CommittedAllNudgedChangesChecker(GetSyncService(0)).Wait());

  // THEMES has no unsynced data.
  EXPECT_FALSE(GetClient(0)
                   ->GetTypesWithUnsyncedDataAndWait({syncer::THEMES})
                   .contains(syncer::THEMES));

  EXPECT_FALSE(
      GetClient(0)->service()->HasCachedPersistentAuthErrorForMetrics());
  histograms.ExpectUniqueSample(
      "Sync.DataTypeNumUnsyncedEntitiesOnReauthFromPendingState.THEME",
      /*sample=*/1, /*expected_bucket_count=*/1);
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Android doesn't currently support PRE_ tests, see crbug.com/1117345.
#if !BUILDFLAG(IS_ANDROID)
class SingleClientFeatureToTransportSyncTest : public SyncTest {
 public:
  SingleClientFeatureToTransportSyncTest() : SyncTest(SINGLE_CLIENT) {
    // Note: kReplaceSyncPromosWithSignInPromos is required so that bookmarks
    // and reading list are considered selected-by-default for non-syncing
    // users.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {syncer::kReadingListEnableSyncTransportModeUponSignIn,
         syncer::kReplaceSyncPromosWithSignInPromos},
        /*disabled_features=*/{});
  }

  ~SingleClientFeatureToTransportSyncTest() override = default;

  void BeforeSetupClient(int index,
                         const base::FilePath& profile_path) override {
    if (!content::IsPreTest()) {
      base::FilePath prefs_path = profile_path.AppendASCII("Preferences");
      std::string prefs_string;
      ASSERT_TRUE(base::ReadFileToString(prefs_path, &prefs_string));
      std::optional<base::Value> prefs = base::JSONReader::Read(
          prefs_string, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
      ASSERT_TRUE(prefs);
      ASSERT_TRUE(prefs->is_dict());
      prefs->GetDict().SetByDottedPath(prefs::kGoogleServicesConsentedToSync,
                                       base::Value(false));

      std::optional<std::string> updated_prefs_string = base::WriteJson(*prefs);
      ASSERT_TRUE(updated_prefs_string);
      ASSERT_TRUE(base::WriteFile(prefs_path, *updated_prefs_string));
    }
  }

  bool SetupClients() override {
    if (!SyncTest::SetupClients()) {
      return false;
    }

    reading_list_helper::WaitForReadingListModelLoaded(reading_list_model());
    return true;
  }

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return SetupSyncMode::kSyncTheFeature;
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
  fake_server_->InjectEntity(
      reading_list_helper::CreateTestReadingListEntity(kUrl, "Title"));

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
  // Sync-the-feature is *not* enabled anymore, and Sync will start up in
  // transport mode instead.
  // Note that this means the persisted metadata is now in an inconsistent
  // state: There is persisted metadata for Sync-the-feature mode, even though
  // Sync is not actually in that mode anymore.
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  ASSERT_EQ(reading_list_model()->size(), 1ul);

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
      syncer::DataTypeHistogramValue(syncer::BOOKMARKS), 1);
  histograms.ExpectBucketCount(
      "Sync.ClearMetadataWhileStopped",
      syncer::DataTypeHistogramValue(syncer::PASSWORDS), 1);
  histograms.ExpectBucketCount(
      "Sync.ClearMetadataWhileStopped",
      syncer::DataTypeHistogramValue(syncer::READING_LIST), 1);
  histograms.ExpectBucketCount(
      "Sync.ClearMetadataWhileStopped",
      syncer::DataTypeHistogramValue(syncer::AUTOFILL_WALLET_DATA), 1);

  // But for data types that use a single model in both transport mode and
  // Sync-the-feature mode (and that support transport mode in the first place),
  // the metadata should *not* have been cleared.
  histograms.ExpectBucketCount(
      "Sync.ClearMetadataWhileStopped",
      syncer::DataTypeHistogramValue(syncer::DEVICE_INFO), 0);
  histograms.ExpectBucketCount(
      "Sync.ClearMetadataWhileStopped",
      syncer::DataTypeHistogramValue(syncer::SHARING_MESSAGE), 0);
  histograms.ExpectBucketCount(
      "Sync.ClearMetadataWhileStopped",
      syncer::DataTypeHistogramValue(syncer::SECURITY_EVENTS), 0);

  // With `kSeparateLocalAndAccountSearchEngines`, the same model is used for
  // both transport mode and Sync-the-feature mode, so the metadata should *not*
  // have been cleared.
  histograms.ExpectBucketCount(
      "Sync.ClearMetadataWhileStopped",
      syncer::DataTypeHistogramValue(syncer::SEARCH_ENGINES),
      base::FeatureList::IsEnabled(
          syncer::kSeparateLocalAndAccountSearchEngines)
          ? 0
          : 1);
}
#endif  // !BUILDFLAG(IS_ANDROID)

class SingleClientPolicySyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SingleClientPolicySyncTest() : SyncTest(SINGLE_CLIENT) {
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      scoped_feature_list_.InitAndEnableFeature(
          syncer::kReplaceSyncPromosWithSignInPromos);
    }
  }
  ~SingleClientPolicySyncTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    SyncTest::SetUpInProcessBrowserTestFixture();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider>*
  policy_provider() {
    return &policy_provider_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SingleClientPolicySyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(SingleClientPolicySyncTest,
                       AppliesSyncTypesListDisabledPolicyImmediately) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::DataType::SEND_TAB_TO_SELF));
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::DataType::BOOKMARKS));

  base::ListValue disabled_types;
  disabled_types.Append("bookmarks");
  policy::PolicyMap policies;
  policies.Set(policy::key::kSyncTypesListDisabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(disabled_types)), nullptr);
  policy_provider()->UpdateChromePolicy(policies);

  // Once the policy is applied, the now-disabled type should not be considered
  // selected anymore.
  ASSERT_FALSE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kBookmarks));

  // Also, Sync should immediately start reconfiguring (without any additional
  // waiting), and as such the now-disabled type should not be active anymore.
  // (Other data types may or may not be active here, depending on timing.)
  EXPECT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::DataType::BOOKMARKS));

  // Wait for some other data type to become active again.
  ASSERT_TRUE(send_tab_to_self_helper::SendTabToSelfActiveChecker(
                  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)))
                  .Wait());
  // The policy-disabled type should still be inactive.
  EXPECT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::DataType::BOOKMARKS));
}

// Regression test for crbug.com/415728693.
// EnterSyncPausedStateForPrimaryAccount() is not supported on Android.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(SingleClientPolicySyncTest,
                       ApplySyncDisabledPolicyWhileSyncPaused) {
  ASSERT_TRUE(SetupSync());

  if (GetSetupSyncMode() == SetupSyncMode::kSyncTheFeature) {
    GetClient(0)->EnterSyncPausedStateForPrimaryAccount();
  } else {
    GetClient(0)->EnterSignInPendingStateForPrimaryAccount();
  }

  ASSERT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  ASSERT_EQ(syncer::GetUploadToGoogleState(GetSyncService(0),
                                           syncer::PRIORITY_PREFERENCES),
            syncer::UploadState::NOT_ACTIVE);

  policy::PolicyMap policies;
  policies.Set(policy::key::kSyncDisabled, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(true), nullptr);
  policy_provider()->UpdateChromePolicy(policies);

  // Once the policy is applied, sync should be disabled.
  ASSERT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::DISABLED);

  // Should not crash.
  ASSERT_EQ(syncer::GetUploadToGoogleState(GetSyncService(0),
                                           syncer::PRIORITY_PREFERENCES),
            syncer::UploadState::NOT_ACTIVE);
}
#endif  // !BUILDFLAG(IS_ANDROID)

class SingleClientOldProgressMarkerSyncTest : public SyncTest {
 public:
  SingleClientOldProgressMarkerSyncTest() : SyncTest(SINGLE_CLIENT) {
    features_.InitWithFeatures(
        /*enabled_features=*/{syncer::kReplaceSyncPromosWithSignInPromos,
#if !BUILDFLAG(IS_ANDROID)
                              syncer::
                                  kReadingListEnableSyncTransportModeUponSignIn,
#endif  // !BUILDFLAG(IS_ANDROID)
                              switches::kSyncEnableBookmarksInTransportMode},
        /*disabled_features=*/{});
  }
  ~SingleClientOldProgressMarkerSyncTest() override = default;
  SingleClientOldProgressMarkerSyncTest(
      const SingleClientOldProgressMarkerSyncTest&) = delete;
  SingleClientOldProgressMarkerSyncTest& operator=(
      const SingleClientOldProgressMarkerSyncTest&) = delete;

  // The value doesn't matter, since the tests use SetupSyncWithMode(..) to
  // explicitly pick Sync-the-feature or Sync-the-transport.
  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return SyncTest::SetupSyncMode::kSyncTransportOnly;
  }

 private:
  base::test::ScopedFeatureList features_;

 protected:
  const base::Uuid kBookmarkUuid1 =
      base::Uuid::ParseLowercase("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa");
  const std::u16string kBookmarkTitle1 = u"title1";
  const std::u16string kBookmarkTitle2 = u"title2";
  const std::u16string kBookmarkTitle3 = u"title3";
  const GURL kBookmarkUrl1 = GURL("https://example1.com");
  const GURL kBookmarkUrl2 = GURL("https://example2.com");
  const GURL kBookmarkUrl3 = GURL("https://example3.com");
  const GURL kReadingListUrl1 = GURL("https://readme1.com/");
  const GURL kReadingListUrl2 = GURL("https://readme2.com/");
  const GURL kReadingListUrl3 = GURL("https://readme3.com/");
};

// TODO(crbug.com/465115079): Enable on Android once PRE_ tests are fully
// supported (currently flakily fails with "Installing ParallelExecutionFence is
// slow", pointing to tasks posted from sync_scheduler_impl.cc).
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SingleClientOldProgressMarkerSyncTest,
                       PRE_OldProgressMarker) {
  ASSERT_TRUE(SetupSyncWithMode(SetupSyncMode::kSyncTransportOnly));

  // Add two bookmarks.
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile(0));
  bookmark_model->AddURL(bookmark_model->account_bookmark_bar_node(), 0,
                         kBookmarkTitle1, kBookmarkUrl1, nullptr, std::nullopt,
                         kBookmarkUuid1);
  bookmark_model->AddURL(bookmark_model->account_bookmark_bar_node(), 0,
                         kBookmarkTitle2, kBookmarkUrl2);

  // Add two reading list entries.
  ReadingListModel* reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(GetProfile(0));
  reading_list_model->AddOrReplaceEntry(kReadingListUrl1, "title1",
                                        reading_list::ADDED_VIA_CURRENT_APP,
                                        /*estimated_read_time=*/std::nullopt,
                                        /*creation_time=*/std::nullopt);
  reading_list_model->AddOrReplaceEntry(kReadingListUrl2, "title2",
                                        reading_list::ADDED_VIA_CURRENT_APP,
                                        /*estimated_read_time=*/std::nullopt,
                                        /*creation_time=*/std::nullopt);

  // Wait for everything to arrive on the server.
  bookmarks_helper::ServerBookmarksEqualityChecker(
      {{kBookmarkTitle1, kBookmarkUrl1}, {kBookmarkTitle2, kBookmarkUrl2}},
      /*cryptographer=*/nullptr)
      .Wait();
  reading_list_helper::ServerReadingListURLsEqualityChecker(
      {kReadingListUrl1, kReadingListUrl2})
      .Wait();

  // Pretend that the last poll happened long ago, so that after restart, a poll
  // will get triggered immediately.
  syncer::SyncTransportDataPrefs prefs(
      GetProfile(0)->GetPrefs(),
      GetClient(0)->GetGaiaIdHashForPrimaryAccount());
  prefs.SetLastPollTime(base::Time::Now() - 10 * prefs.GetPollInterval());
}

IN_PROC_BROWSER_TEST_F(SingleClientOldProgressMarkerSyncTest,
                       OldProgressMarker) {
  // While the client is offline, some server-side changes happen: The first
  // bookmark is deleted, and a third one is added. The second one remains
  // unchanged.
  GetFakeServer()->InjectEntity(
      CreateTombstone(syncer::BOOKMARKS, kBookmarkUuid1.AsLowercaseString()));
  GetFakeServer()->InjectEntity(bookmarks_helper::CreateBookmarkServerEntity(
      kBookmarkTitle3, kBookmarkUrl3));
  // Same for the reading list entries: The first gets deleted, and a third gets
  // added.
  GetFakeServer()->InjectEntity(
      CreateTombstone(syncer::READING_LIST, kReadingListUrl1.spec()));
  GetFakeServer()->InjectEntity(
      reading_list_helper::CreateTestReadingListEntity(kReadingListUrl3,
                                                       "new title"));

  // The client is offline for so long that its progress markers are no longer
  // usable. This means the server will send a full update, with a "clear all"
  // GC directive, instead of a regular incremental update.
  GetFakeServer()->SetRejectOldProgressMarkerForType(syncer::BOOKMARKS);
  GetFakeServer()->SetRejectOldProgressMarkerForType(syncer::READING_LIST);

  base::HistogramTester histograms;

  // Now the client comes online again. This should trigger a poll request,
  // since the last poll time was long ago.
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  // Verify that the changes were applied. Note that the outcome here is the
  // same as if the server had sent a regular incremental update.
  bookmarks_helper::BookmarksUrlChecker(0, kBookmarkUrl1, 0).Wait();
  bookmarks_helper::BookmarksUrlChecker(0, kBookmarkUrl2, 1).Wait();
  bookmarks_helper::BookmarksUrlChecker(0, kBookmarkUrl3, 1).Wait();

  ReadingListModel* reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(GetProfile(0));
  reading_list_helper::LocalReadingListURLsEqualityChecker(
      reading_list_model, {kReadingListUrl2, kReadingListUrl3})
      .Wait();

  // Verify via histograms that the server indeed sent a full update, not an
  // incremental one - in particular, that it did not send any tombstones. Note
  // that the DataTypeEntityChange histograms are recorded at a low level (in
  // the worker), and represent what the server actually sent to the client,
  // *not* what was sent to the bridge.
  // Note 1: For the purpose of this histogram, the updates are still considered
  // *non*-initial, since the client didn't trigger an initial sync.
  // Note 2: For bookmarks, the server also returns the root node plus the 3
  // permanent nodes, so together with the 2 "real" updates there are 6 total
  // updates.
  EXPECT_THAT(histograms.GetAllSamples("Sync.DataTypeEntityChange.BOOKMARK"),
              ElementsAre(base::Bucket(
                  syncer::DataTypeEntityChange::kRemoteNonInitialUpdate, 6)));
  EXPECT_THAT(
      histograms.GetAllSamples("Sync.DataTypeEntityChange.READING_LIST"),
      ElementsAre(base::Bucket(
          syncer::DataTypeEntityChange::kRemoteNonInitialUpdate, 2)));
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
