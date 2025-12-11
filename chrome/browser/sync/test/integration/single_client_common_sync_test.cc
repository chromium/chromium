// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/enum_set.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
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
#include "chrome/browser/sync/test/integration/send_tab_to_self_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/themes_helper.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/reading_list/core/dual_reading_list_model.h"
#include "components/reading_list/core/mock_reading_list_model_observer.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/time.h"
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

class SingleClientCommonSyncTest : public SyncTest {
 public:
  SingleClientCommonSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientCommonSyncTest() override = default;
  SingleClientCommonSyncTest(const SingleClientCommonSyncTest&) = delete;
  SingleClientCommonSyncTest& operator=(const SingleClientCommonSyncTest&) =
      delete;
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
IN_PROC_BROWSER_TEST_F(SingleClientCommonSyncTest,
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

IN_PROC_BROWSER_TEST_F(SingleClientCommonSyncTest,
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

class SingleClientGetUnsyncedTypesTest : public SingleClientCommonSyncTest {
 public:
  SingleClientGetUnsyncedTypesTest() {
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

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SingleClientGetUnsyncedTypesTest,
                       ShouldGetTypesWithUnsyncedDataFromSyncService) {
  // Sign in.
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());

#if !BUILDFLAG(IS_ANDROID)
  // Enable account storage for bookmarks.
  SigninPrefs prefs(*GetProfile(0)->GetPrefs());
  const GaiaId gaia_id = GetSyncService(0)->GetSyncAccountInfoForPrefs().gaia;
  prefs.SetBookmarksExplicitBrowserSignin(gaia_id, true);
  ASSERT_TRUE(prefs.GetBookmarksExplicitBrowserSignin(gaia_id));
#endif  // !BUILDFLAG(IS_ANDROID)

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().HasAll({syncer::BOOKMARKS}));

  // BOOKMARKS has no unsynced data.
  EXPECT_FALSE(GetClient(0)
                   ->GetTypesWithUnsyncedDataAndWait({syncer::BOOKMARKS})
                   .contains(syncer::BOOKMARKS));

  ASSERT_TRUE(bookmarks_helper::BookmarkModelMatchesFakeServerChecker(
                  GetBookmarkModel(0), GetSyncService(0), GetFakeServer())
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
  // Sign in.
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());

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

  // Sign in.
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());

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

class SingleClientPolicySyncTest : public SyncTest {
 public:
  SingleClientPolicySyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientPolicySyncTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    SyncTest::SetUpInProcessBrowserTestFixture();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider>*
  policy_provider() {
    return &policy_provider_;
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(SingleClientPolicySyncTest,
                       AppliesSyncTypesListDisabledPolicyImmediately) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::DataType::SEND_TAB_TO_SELF));
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::DataType::BOOKMARKS));

  base::Value::List disabled_types;
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
IN_PROC_BROWSER_TEST_F(SingleClientPolicySyncTest,
                       ApplySyncDisabledPolicyWhileSyncPaused) {
  ASSERT_TRUE(SetupSync());

  GetClient(0)->EnterSyncPausedStateForPrimaryAccount();
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

}  // namespace
