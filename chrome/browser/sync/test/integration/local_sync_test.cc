// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/test_matchers.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/protocol/loopback_server.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync_bookmarks/switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"

// The local sync backend is currently only supported on Windows, Mac, Linux.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

namespace {

using bookmarks::test::IsFolderWithUuid;
using bookmarks::test::IsUrlBookmarkWithUuid;
using syncer::SyncServiceImpl;
using testing::ElementsAre;

constexpr char kTestPassphrase[] = "hunter2";

class SyncTransportActiveChecker : public SingleClientStatusChangeChecker {
 public:
  explicit SyncTransportActiveChecker(SyncServiceImpl* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for sync transport to become active";
    return service()->GetTransportState() ==
           syncer::SyncService::TransportState::ACTIVE;
  }
};

class UpdatedBirthdayChecker : public SingleClientStatusChangeChecker {
 public:
  UpdatedBirthdayChecker(SyncServiceImpl* service,
                         const std::string& old_birthday)
      : SingleClientStatusChangeChecker(service), old_birthday_(old_birthday) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for store birthday to change from " << old_birthday_;
    const std::string birthday =
        service()->GetLastCycleSnapshotForDebugging().birthday();
    return !birthday.empty() && birthday != old_birthday_;
  }

 private:
  const std::string old_birthday_;
};

// This test verifies some basic functionality of local sync, used for roaming
// profiles (enterprise use-case).
class LocalSyncTest : public InProcessBrowserTest {
 public:
  LocalSyncTest(const LocalSyncTest&) = delete;
  LocalSyncTest& operator=(const LocalSyncTest&) = delete;

 protected:
  LocalSyncTest() = default;
  ~LocalSyncTest() override = default;

  static base::FilePath GetLocalSyncBackendDir(
      const base::CommandLine* command_line) {
    const base::FilePath user_data_dir =
        command_line->GetSwitchValuePath(switches::kUserDataDir);
    CHECK(!user_data_dir.empty());
    return user_data_dir.Append(FILE_PATH_LITERAL("profile.pb"));
  }

  static base::FilePath GetLocalSyncBackendFilePath(
      const base::CommandLine* command_line) {
    base::FilePath file = GetLocalSyncBackendDir(command_line);
#if BUILDFLAG(IS_WIN)
    // On Windows, `ChromeSyncClient::GetLocalSyncBackendFolder()` appends the
    // profile directory name and "profile.pb" to `kLocalSyncBackendDir`.
    file = file.AppendASCII(chrome::kInitialProfile)
               .Append(FILE_PATH_LITERAL("profile.pb"));
#endif  // BUILDFLAG(IS_WIN)
    return file;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // By default on Windows OS local sync backend uses roaming profile. It can
    // lead to problems if some tests run simultaneously and use the same
    // roaming profile.
    const base::FilePath dir = GetLocalSyncBackendDir(command_line);
    command_line->AppendSwitchASCII(switches::kLocalSyncBackendDir,
                                    dir.MaybeAsASCII());
    command_line->AppendSwitch(switches::kEnableLocalSyncBackend);
    command_line->AppendSwitchASCII(syncer::kSyncDeferredStartupTimeoutSeconds,
                                    "0");
  }
};

IN_PROC_BROWSER_TEST_F(LocalSyncTest, ShouldStart) {
  SyncServiceImpl* service =
      SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
          browser()->GetProfile());

  // Wait until the first sync cycle is completed.
  ASSERT_TRUE(SyncTransportActiveChecker(service).Wait());

  EXPECT_TRUE(service->IsLocalSyncEnabled());
  EXPECT_FALSE(service->IsSyncFeatureEnabled());
  EXPECT_FALSE(service->IsSyncFeatureActive());
  EXPECT_FALSE(service->GetUserSettings()->IsInitialSyncFeatureSetupComplete());

  // Verify that the expected set of data types successfully started up.
  // If this test fails after adding a new data type, carefully consider whether
  // the type should be enabled in Local Sync mode, i.e. for roaming profiles on
  // Windows.
  syncer::DataTypeSet expected_active_data_types = {
      syncer::BOOKMARKS,
      syncer::READING_LIST,
      syncer::PREFERENCES,
      syncer::PASSWORDS,
      syncer::AUTOFILL_PROFILE,
      syncer::AUTOFILL,
      syncer::THEMES,
      syncer::EXTENSIONS,
      syncer::SAVED_TAB_GROUP,
      syncer::SEARCH_ENGINES,
      syncer::SESSIONS,
      syncer::APPS,
      syncer::APP_SETTINGS,
      syncer::EXTENSION_SETTINGS,
      syncer::DEVICE_INFO,
      syncer::PRIORITY_PREFERENCES,
      syncer::WEBAUTHN_CREDENTIAL,
      syncer::WEB_APPS,
      syncer::NIGORI};

  expected_active_data_types.Put(syncer::AUTOFILL_WALLET_CREDENTIAL);

  // The dictionary is currently only synced on Windows and Linux.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  expected_active_data_types.Put(syncer::DICTIONARY);
#endif
  EXPECT_EQ(service->GetActiveDataTypes(), expected_active_data_types);

  // Verify certain features are disabled.
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::USER_CONSENTS));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::USER_EVENTS));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::SECURITY_EVENTS));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::SEND_TAB_TO_SELF));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::SHARING_MESSAGE));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::SEND_TAB_TO_SELF));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::HISTORY));
}

IN_PROC_BROWSER_TEST_F(LocalSyncTest, ShouldHonorSelectedTypes) {
  SyncServiceImpl* service =
      SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
          browser()->GetProfile());

  // Wait until the first sync cycle is completed.
  ASSERT_TRUE(SyncTransportActiveChecker(service).Wait());

  ASSERT_TRUE(service->IsLocalSyncEnabled());
  ASSERT_FALSE(service->IsSyncFeatureEnabled());
  ASSERT_TRUE(service->GetActiveDataTypes().Has(syncer::BOOKMARKS));
  ASSERT_TRUE(service->GetActiveDataTypes().Has(syncer::PASSWORDS));

  service->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, {syncer::UserSelectableType::kPasswords});

  ASSERT_TRUE(SyncTransportActiveChecker(service).Wait());

  EXPECT_TRUE(service->GetActiveDataTypes().Has(syncer::PASSWORDS));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::BOOKMARKS));
}

// Setting up a custom passphrase is arguably meaningless for local sync, but it
// has been allowed historically.
IN_PROC_BROWSER_TEST_F(LocalSyncTest, ShouldSupportCustomPassphrase) {
  SyncServiceImpl* service =
      SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
          browser()->GetProfile());

  // Wait until the first sync cycle is completed.
  ASSERT_TRUE(SyncTransportActiveChecker(service).Wait());

  ASSERT_TRUE(service->IsLocalSyncEnabled());
  ASSERT_FALSE(service->IsSyncFeatureEnabled());

  service->GetUserSettings()->SetEncryptionPassphrase(kTestPassphrase);

  EXPECT_TRUE(PassphraseAcceptedChecker(service).Wait());
}

IN_PROC_BROWSER_TEST_F(LocalSyncTest, ShouldReportNoLocalOnlyData) {
  SyncServiceImpl* service =
      SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
          browser()->GetProfile());

  // Wait until the first sync cycle is completed.
  ASSERT_TRUE(SyncTransportActiveChecker(service).Wait());
  ASSERT_TRUE(service->IsLocalSyncEnabled());
  ASSERT_FALSE(service->HasSyncConsent());

  base::test::TestFuture<absl::flat_hash_map<syncer::DataType, size_t>>
      types_with_unsynced_data;
  service->GetTypesWithUnsyncedData({syncer::BOOKMARKS},
                                    types_with_unsynced_data.GetCallback());
  EXPECT_TRUE(types_with_unsynced_data.Get().empty());

  base::test::TestFuture<
      std::map<syncer::DataType, syncer::LocalDataDescription>>
      descriptions;
  service->GetLocalDataDescriptions({syncer::BOOKMARKS},
                                    descriptions.GetCallback());
  EXPECT_TRUE(descriptions.Get().empty());
}

class LocalSyncBookmarkMigrationTest : public LocalSyncTest {
 public:
  LocalSyncBookmarkMigrationTest() {
    // Disable `kSyncReuploadBookmarks` as it would otherwise cause the client
    // to reupload legacy bookmarks with modern specifics during the PRE_ test,
    // interfering with the server-side migration behavior verified by the test.
    if (content::IsPreTest()) {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{
              syncer::kSyncMigrateLoopbackServerBookmarksToClientTagHash,
              switches::kSyncReuploadBookmarks});
    } else {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {syncer::kSyncMigrateLoopbackServerBookmarksToClientTagHash},
          /*disabled_features=*/{switches::kSyncReuploadBookmarks});
    }
  }

  ~LocalSyncBookmarkMigrationTest() override = default;

  bool SetUpUserDataDirectory() override {
    if (!LocalSyncTest::SetUpUserDataDirectory()) {
      return false;
    }

    if (!content::IsPreTest()) {
      // In the non-PRE_ test, preserve the data on disk from the PRE_ run.
      return true;
    }

    const base::FilePath profile_pb_path =
        GetLocalSyncBackendFilePath(base::CommandLine::ForCurrentProcess());
    if (!base::CreateDirectory(profile_pb_path.DirName())) {
      return false;
    }

    sync_pb::LoopbackServerProto proto;
    proto.set_version(1);
    proto.set_store_birthday(kInitialStoreBirthday);
    proto.set_last_version_assigned(10);
    proto.add_keystore_keys(std::string(32, 'k'));

    // Nigori permanent entity.
    sync_pb::LoopbackServerEntity* nigori_entity = proto.add_entities();
    nigori_entity->set_type(sync_pb::LoopbackServerEntity_Type_PERMANENT);
    sync_pb::SyncEntity* nigori_sync_entity = nigori_entity->mutable_entity();
    nigori_sync_entity->set_id_string("39247_google_chrome_nigori");
    nigori_sync_entity->set_server_defined_unique_tag("google_chrome_nigori");
    nigori_sync_entity->set_name("google_chrome_nigori");
    nigori_sync_entity->set_version(1);
    nigori_sync_entity->mutable_specifics()->mutable_nigori();

    // Bookmark root permanent entity.
    sync_pb::LoopbackServerEntity* bookmark_root = proto.add_entities();
    bookmark_root->set_type(sync_pb::LoopbackServerEntity_Type_PERMANENT);
    sync_pb::SyncEntity* bookmark_root_sync = bookmark_root->mutable_entity();
    bookmark_root_sync->set_id_string("32904_google_chrome_bookmarks");
    bookmark_root_sync->set_server_defined_unique_tag(
        "google_chrome_bookmarks");
    bookmark_root_sync->set_name("google_chrome_bookmarks");
    bookmark_root_sync->set_version(1);
    bookmark_root_sync->mutable_specifics()->mutable_bookmark();

    // 1. Bookmark Bar permanent folder.
    sync_pb::LoopbackServerEntity* bar_entity = proto.add_entities();
    bar_entity->set_type(sync_pb::LoopbackServerEntity_Type_PERMANENT);
    sync_pb::SyncEntity* bar_sync_entity = bar_entity->mutable_entity();
    bar_sync_entity->set_id_string("32904_bookmark_bar");
    bar_sync_entity->set_server_defined_unique_tag("bookmark_bar");
    bar_sync_entity->set_parent_id_string("32904_google_chrome_bookmarks");
    bar_sync_entity->set_name("Bookmark Bar");
    bar_sync_entity->set_version(1);
    bar_sync_entity->mutable_specifics()->mutable_bookmark();

    // Other bookmarks permanent folder.
    sync_pb::LoopbackServerEntity* other_entity = proto.add_entities();
    other_entity->set_type(sync_pb::LoopbackServerEntity_Type_PERMANENT);
    sync_pb::SyncEntity* other_sync_entity = other_entity->mutable_entity();
    other_sync_entity->set_id_string("32904_other_bookmarks");
    other_sync_entity->set_server_defined_unique_tag("other_bookmarks");
    other_sync_entity->set_parent_id_string("32904_google_chrome_bookmarks");
    other_sync_entity->set_name("Other Bookmarks");
    other_sync_entity->set_version(1);
    other_sync_entity->mutable_specifics()->mutable_bookmark();

    // Synced bookmarks permanent folder.
    sync_pb::LoopbackServerEntity* synced_entity = proto.add_entities();
    synced_entity->set_type(sync_pb::LoopbackServerEntity_Type_PERMANENT);
    sync_pb::SyncEntity* synced_sync_entity = synced_entity->mutable_entity();
    synced_sync_entity->set_id_string("32904_synced_bookmarks");
    synced_sync_entity->set_server_defined_unique_tag("synced_bookmarks");
    synced_sync_entity->set_parent_id_string("32904_google_chrome_bookmarks");
    synced_sync_entity->set_name("Mobile Bookmarks");
    synced_sync_entity->set_version(1);
    synced_sync_entity->mutable_specifics()->mutable_bookmark();

    // 2. Legacy bookmark folder in Other Bookmarks (without `parent_guid` in
    // specifics, exercising the fallback to `kOtherBookmarksFolderUuid`).
    sync_pb::LoopbackServerEntity* folder_entity = proto.add_entities();
    folder_entity->set_type(sync_pb::LoopbackServerEntity_Type_BOOKMARK);
    sync_pb::SyncEntity* folder_sync_entity = folder_entity->mutable_entity();
    folder_sync_entity->set_id_string("32904_" + std::string(kFolderGuid));
    folder_sync_entity->set_version(1);
    folder_sync_entity->set_name("My Folder");
    folder_sync_entity->set_folder(true);
    folder_sync_entity->set_parent_id_string("32904_other_bookmarks");
    folder_sync_entity->set_originator_cache_guid("legacy_cache_guid");
    folder_sync_entity->set_originator_client_item_id(kFolderGuid);
    *folder_sync_entity->mutable_unique_position() =
        syncer::UniquePosition::InitialPosition(
            syncer::UniquePosition::RandomSuffix())
            .ToProto();
    folder_sync_entity->mutable_specifics()->mutable_bookmark();

    // 3. Legacy bookmark inside the folder (with `parent_guid` in specifics).
    sync_pb::LoopbackServerEntity* bookmark_entity = proto.add_entities();
    bookmark_entity->set_type(sync_pb::LoopbackServerEntity_Type_BOOKMARK);
    sync_pb::SyncEntity* bookmark_sync_entity =
        bookmark_entity->mutable_entity();
    bookmark_sync_entity->set_id_string("32904_" + std::string(kBookmarkGuid));
    bookmark_sync_entity->set_version(2);
    bookmark_sync_entity->set_name("Google");
    bookmark_sync_entity->set_folder(false);
    bookmark_sync_entity->set_parent_id_string("32904_" +
                                               std::string(kFolderGuid));
    bookmark_sync_entity->set_originator_cache_guid("legacy_cache_guid");
    bookmark_sync_entity->set_originator_client_item_id(kBookmarkGuid);
    *bookmark_sync_entity->mutable_unique_position() =
        syncer::UniquePosition::InitialPosition(
            syncer::UniquePosition::RandomSuffix())
            .ToProto();
    bookmark_sync_entity->mutable_specifics()->mutable_bookmark()->set_url(
        "https://www.google.com/");
    bookmark_sync_entity->mutable_specifics()
        ->mutable_bookmark()
        ->set_parent_guid(kFolderGuid);

    std::string serialized;
    if (!proto.SerializeToString(&serialized)) {
      return false;
    }
    return base::WriteFile(profile_pb_path, serialized);
  }

 protected:
  static constexpr int64_t kInitialStoreBirthday = 11111;
  static constexpr char kFolderGuid[] = "11111111-1111-4111-8111-111111111111";
  static constexpr char kBookmarkGuid[] =
      "22222222-2222-4222-8222-222222222222";

  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(LocalSyncBookmarkMigrationTest,
                       PRE_ShouldMigrateLegacyBookmarksOnStartup) {
  SyncServiceImpl* service =
      SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
          browser()->GetProfile());

  // Wait until sync transport is active and initial sync completes.
  ASSERT_TRUE(SyncTransportActiveChecker(service).Wait());
  ASSERT_EQ(service->GetLastCycleSnapshotForDebugging().birthday(),
            base::NumberToString(kInitialStoreBirthday));

  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile());
  ASSERT_TRUE(model);

  EXPECT_THAT(model->other_node()->children(),
              ElementsAre(IsFolderWithUuid(
                  u"My Folder", base::Uuid::ParseLowercase(kFolderGuid),
                  ElementsAre(IsUrlBookmarkWithUuid(
                      u"Google", GURL("https://www.google.com/"),
                      base::Uuid::ParseLowercase(kBookmarkGuid))))));

  // Legacy data in LoopbackServer didn't have UUIDs in specifics, so the client
  // had to read them from originator client item ID (OCII).
  EXPECT_EQ(2, histogram_tester_.GetBucketCount("Sync.BookmarkGUIDSource2",
                                                /*kValidOCII=*/1));
  EXPECT_EQ(0, histogram_tester_.GetBucketCount("Sync.BookmarkGUIDSource2",
                                                /*kSpecifics=*/0));
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(base::PathExists(
      GetLocalSyncBackendFilePath(base::CommandLine::ForCurrentProcess())
          .AddExtension(FILE_PATH_LITERAL("bak"))));
}

IN_PROC_BROWSER_TEST_F(LocalSyncBookmarkMigrationTest,
                       ShouldMigrateLegacyBookmarksOnStartup) {
  SyncServiceImpl* service =
      SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
          browser()->GetProfile());

  // Wait until NOT_MY_BIRTHDAY is handled (which stops the initial engine and
  // clears local sync metadata) and the restarted engine completes a sync cycle
  // with the new store birthday and becomes active.
  ASSERT_TRUE(UpdatedBirthdayChecker(
                  service, base::NumberToString(kInitialStoreBirthday))
                  .Wait());
  ASSERT_TRUE(SyncTransportActiveChecker(service).Wait());

  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile());
  ASSERT_TRUE(model);

  EXPECT_THAT(model->other_node()->children(),
              ElementsAre(IsFolderWithUuid(
                  u"My Folder", base::Uuid::ParseLowercase(kFolderGuid),
                  ElementsAre(IsUrlBookmarkWithUuid(
                      u"Google", GURL("https://www.google.com/"),
                      base::Uuid::ParseLowercase(kBookmarkGuid))))));

  // LoopbackServer migrated legacy bookmarks to modern schema with GUID and
  // parent GUID in specifics, bumped birthday, and the client downloaded
  // modern specifics upon NOT_MY_BIRTHDAY.
  EXPECT_EQ(2, histogram_tester_.GetBucketCount("Sync.BookmarkGUIDSource2",
                                                /*kSpecifics=*/0));
  EXPECT_EQ(0, histogram_tester_.GetBucketCount("Sync.BookmarkGUIDSource2",
                                                /*kValidOCII=*/1));
  EXPECT_EQ(2, histogram_tester_.GetBucketCount("Sync.BookmarkParentGuidSource",
                                                /*kFoundInSpecifics=*/2));
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(
      GetLocalSyncBackendFilePath(base::CommandLine::ForCurrentProcess())
          .AddExtension(FILE_PATH_LITERAL("bak"))));
}

}  // namespace

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
