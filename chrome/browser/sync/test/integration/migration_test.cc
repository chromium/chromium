// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/browser_test.h"

using bookmarks_helper::AddURL;
using bookmarks_helper::GetUniqueNodeByURL;
using bookmarks_helper::IndexedURL;
using bookmarks_helper::IndexedURLTitle;
using bookmarks_helper::ServerBookmarksEqualityChecker;
using bookmarks_helper::SetTitle;
using bookmarks_helper::StoreType;

using preferences_helper::BooleanPrefMatches;
using preferences_helper::ChangeBooleanPref;

namespace {

std::optional<sync_pb::SyncEntity> FindBookmarkEntityByURL(
    fake_server::FakeServer* fake_server,
    const GURL& url) {
  const std::vector<sync_pb::SyncEntity> entities =
      fake_server->GetSyncEntitiesByDataType(syncer::BOOKMARKS);
  for (const auto& entity : entities) {
    if (entity.specifics().bookmark().url() == url.spec()) {
      return entity;
    }
  }
  return std::nullopt;
}

std::optional<sync_pb::SyncEntity> FindPreferenceEntityByPrefName(
    fake_server::FakeServer* fake_server,
    const std::string& pref_name) {
  const std::vector<sync_pb::SyncEntity> entities =
      fake_server->GetSyncEntitiesByDataType(syncer::PREFERENCES);
  for (const auto& entity : entities) {
    if (entity.specifics().preference().name() == pref_name) {
      return entity;
    }
  }
  return std::nullopt;
}

// Utility functions to make a data type set out of a small number of
// data types.

// TODO(crbug.com/40911681): MakeSet() seems pretty redundant, can be replaced
// with its body.
syncer::DataTypeSet MakeSet(syncer::DataType type) {
  return {type};
}

syncer::DataTypeSet MakeSet(syncer::DataType type1, syncer::DataType type2) {
  return {type1, type2};
}

// An ordered list of data types sets to migrate.  Used by
// RunMigrationTest().
using MigrationList = base::circular_deque<syncer::DataTypeSet>;

// Utility functions to make a MigrationList out of a small number of
// data types / data type sets.

MigrationList MakeList(syncer::DataTypeSet data_types) {
  return MigrationList(1, data_types);
}

MigrationList MakeList(syncer::DataTypeSet data_types1,
                       syncer::DataTypeSet data_types2) {
  MigrationList migration_list;
  migration_list.push_back(data_types1);
  migration_list.push_back(data_types2);
  return migration_list;
}

MigrationList MakeList(syncer::DataType type) {
  return MakeList(MakeSet(type));
}

MigrationList MakeList(syncer::DataType type1, syncer::DataType type2) {
  return MakeList(MakeSet(type1), MakeSet(type2));
}

class MigrationCompletionChecker : public SingleClientStatusChangeChecker {
 public:
  MigrationCompletionChecker(syncer::SyncServiceImpl* service,
                             fake_server::FakeServer* fake_server,
                             syncer::DataTypeSet expected_types)
      : SingleClientStatusChangeChecker(service),
        fake_server_(fake_server),
        expected_types_(expected_types) {}

  ~MigrationCompletionChecker() override = default;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for migration of "
        << syncer::DataTypeSetToDebugString(expected_types_) << ". ";

    if (service()->GetTransportState() !=
        syncer::SyncService::TransportState::ACTIVE) {
      *os << "Transport state is "
          << static_cast<int>(service()->GetTransportState())
          << " (waiting for ACTIVE).";
      return false;
    }

    for (syncer::DataType type : expected_types_) {
      if (!service()->GetActiveDataTypes().Has(type)) {
        *os << syncer::DataTypeToDebugString(type) << " is not active.";
        return false;
      }

      std::optional<int> client_version =
          GetMigrationVersionFromProgressMarker(type);
      if (!client_version) {
        *os << "No valid progress marker for "
            << syncer::DataTypeToDebugString(type) << ".";
        return false;
      }

      const int server_version = fake_server_->GetMigrationVersion(type);

      if (*client_version != server_version) {
        *os << syncer::DataTypeToDebugString(type)
            << " has client migration version " << *client_version
            << " but server expects " << server_version << ".";
        return false;
      }
    }

    return true;
  }

 private:
  std::optional<int> GetMigrationVersionFromProgressMarker(
      const syncer::DataType type) {
    const syncer::SyncCycleSnapshot& snap =
        service()->GetLastCycleSnapshotForDebugging();
    const syncer::ProgressMarkerMap& markers = snap.download_progress_markers();
    const auto it = markers.find(type);
    if (it == markers.end()) {
      return std::nullopt;
    }
    sync_pb::DataTypeProgressMarker marker_proto;
    if (!marker_proto.ParseFromString(it->second)) {
      return std::nullopt;
    }
    return fake_server::FakeServer::GetProgressMarkerMigrationVersion(
        marker_proto);
  }

  const raw_ptr<fake_server::FakeServer> fake_server_;
  const syncer::DataTypeSet expected_types_;
};

class MigrationTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  explicit MigrationTest(TestType test_type) : SyncTest(test_type) {
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      scoped_feature_list_.InitWithFeatures(
          {syncer::kReplaceSyncPromosWithSignInPromos,
           syncer::kSpellcheckSeparateLocalAndAccountDictionaries},
          {});
    }
  }

  MigrationTest(const MigrationTest&) = delete;
  MigrationTest& operator=(const MigrationTest&) = delete;

  ~MigrationTest() override = default;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

  enum TriggerMethod { MODIFY_PREF, MODIFY_BOOKMARK, TRIGGER_REFRESH };

  syncer::DataTypeSet GetPreferredDataTypes() {
    // SyncServiceImpl must already have been created before we can call
    // GetPreferredDataTypes().
    DCHECK(GetSyncService(0));
    syncer::DataTypeSet preferred_data_types =
        GetSyncService(0)->GetPreferredDataTypes();

    // Make sure all clients have the same preferred data types.
    for (int i = 1; i < num_clients(); ++i) {
      const syncer::DataTypeSet other_preferred_data_types =
          GetSyncService(i)->GetPreferredDataTypes();
      EXPECT_EQ(other_preferred_data_types, preferred_data_types);
    }

    preferred_data_types.RetainAll(syncer::ProtocolTypes());

    // Supervised user data types will be "unready" during this test, so we
    // should not request that they be migrated.
    preferred_data_types.Remove(syncer::SUPERVISED_USER_SETTINGS);

    // Autofill wallet and plus address will be unready during this test, so we
    // should not request that it be migrated.
    preferred_data_types.RemoveAll(
        {syncer::AUTOFILL_WALLET_DATA, syncer::AUTOFILL_WALLET_METADATA,
         syncer::AUTOFILL_WALLET_OFFER, syncer::PLUS_ADDRESS,
         syncer::PLUS_ADDRESS_SETTING});

    // ARC package will be unready during this test, so we should not request
    // that it be migrated.
    preferred_data_types.Remove(syncer::ARC_PACKAGE);

    // Doesn't make sense to migrate commit only types.
    preferred_data_types.RemoveAll(syncer::CommitOnlyTypes());

    return preferred_data_types;
  }

  // Returns a MigrationList with every enabled data type in its own
  // set.
  MigrationList GetPreferredDataTypesList() {
    MigrationList migration_list;
    const syncer::DataTypeSet preferred_data_types = GetPreferredDataTypes();
    for (syncer::DataType type : preferred_data_types) {
      migration_list.push_back(MakeSet(type));
    }
    return migration_list;
  }

  StoreType GetStoreType() {
    return GetSetupSyncMode() == SyncTest::SetupSyncMode::kSyncTransportOnly
               ? StoreType::kAccountStore
               : StoreType::kLocalOrSyncableStore;
  }

  // Trigger a migration for the given types with the given method.
  void TriggerMigration(syncer::DataTypeSet data_types,
                        TriggerMethod trigger_method) {
    switch (trigger_method) {
      case MODIFY_PREF:
        // Unlike MODIFY_BOOKMARK, MODIFY_PREF doesn't cause a
        // notification to happen (since model association on a
        // boolean pref clobbers the local value), so it doesn't work
        // for anything but single-client tests.
        ASSERT_EQ(1, num_clients());
        ChangeBooleanPref(0, prefs::kShowHomeButton);
        break;
      case MODIFY_BOOKMARK:
        ASSERT_TRUE(
            AddURL(0, IndexedURLTitle(0), GURL(IndexedURL(0)), GetStoreType()));
        break;
      case TRIGGER_REFRESH:
        TriggerSyncForDataTypes(/*index=*/0, data_types);
        break;
      default:
        ADD_FAILURE();
    }
  }

  // Block until all clients have completed migration for the given
  // types.
  void AwaitMigration(syncer::DataTypeSet migrate_types) {
    for (int i = 0; i < num_clients(); ++i) {
      ASSERT_TRUE(MigrationCompletionChecker(GetSyncService(i), GetFakeServer(),
                                             migrate_types)
                      .Wait());
    }
  }

  // Makes sure migration works with the given migration list and
  // trigger method.
  void RunMigrationTest(const MigrationList& migration_list,
                        TriggerMethod trigger_method) {
    // Phase 1: Trigger the migrations on the server.
    for (const syncer::DataTypeSet& data_types : migration_list) {
      TriggerMigrationDoneError(data_types);
    }

    // Phase 2: Trigger each migration individually and wait for it to
    // complete.  (Multiple migrations may be handled by each
    // migration cycle, but there's no guarantee of that, so we have
    // to trigger each migration individually.)
    for (const syncer::DataTypeSet& data_types : migration_list) {
      TriggerMigration(data_types, trigger_method);
      AwaitMigration(data_types);
    }

    // Phase 3: Wait for all clients to catch up.
    ASSERT_TRUE(AwaitQuiescence());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class MigrationSingleClientTest : public MigrationTest {
 public:
  MigrationSingleClientTest() : MigrationTest(SINGLE_CLIENT) {}

  MigrationSingleClientTest(const MigrationSingleClientTest&) = delete;
  MigrationSingleClientTest& operator=(const MigrationSingleClientTest&) =
      delete;

  ~MigrationSingleClientTest() override = default;

  void RunSingleClientMigrationTest(const MigrationList& migration_list,
                                    TriggerMethod trigger_method) {
    ASSERT_TRUE(SetupSync());
    RunMigrationTest(migration_list, trigger_method);
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         MigrationSingleClientTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

// The simplest possible migration tests -- a single data type.

IN_PROC_BROWSER_TEST_P(MigrationSingleClientTest, PrefsOnlyModifyPref) {
  RunSingleClientMigrationTest(MakeList(syncer::PREFERENCES), MODIFY_PREF);
}

IN_PROC_BROWSER_TEST_P(MigrationSingleClientTest, PrefsOnlyModifyBookmark) {
  RunSingleClientMigrationTest(MakeList(syncer::PREFERENCES), MODIFY_BOOKMARK);
}

IN_PROC_BROWSER_TEST_P(MigrationSingleClientTest, PrefsOnlyTriggerRefresh) {
  RunSingleClientMigrationTest(MakeList(syncer::PREFERENCES), TRIGGER_REFRESH);
}

// Nigori is handled specially, so we test that separately.

IN_PROC_BROWSER_TEST_P(MigrationSingleClientTest, NigoriOnly) {
  RunSingleClientMigrationTest(MakeList(syncer::PREFERENCES), TRIGGER_REFRESH);
}

// A little more complicated -- two data types.

IN_PROC_BROWSER_TEST_P(MigrationSingleClientTest, BookmarksPrefsIndividually) {
  RunSingleClientMigrationTest(MakeList(syncer::BOOKMARKS, syncer::PREFERENCES),
                               MODIFY_PREF);
}

IN_PROC_BROWSER_TEST_P(MigrationSingleClientTest, BookmarksPrefsBoth) {
  RunSingleClientMigrationTest(
      MakeList(MakeSet(syncer::BOOKMARKS, syncer::PREFERENCES)),
      MODIFY_BOOKMARK);
}

// Two data types with one being nigori.

IN_PROC_BROWSER_TEST_P(MigrationSingleClientTest, PrefsNigoriIndividiaully) {
  RunSingleClientMigrationTest(MakeList(syncer::PREFERENCES, syncer::NIGORI),
                               TRIGGER_REFRESH);
}

IN_PROC_BROWSER_TEST_P(MigrationSingleClientTest, PrefsNigoriBoth) {
  RunSingleClientMigrationTest(
      MakeList(MakeSet(syncer::PREFERENCES, syncer::NIGORI)), MODIFY_PREF);
}

// The whole shebang -- all data types.
IN_PROC_BROWSER_TEST_P(MigrationSingleClientTest, AllTypesIndividually) {
  ASSERT_TRUE(SetupClients());
  RunSingleClientMigrationTest(GetPreferredDataTypesList(), MODIFY_BOOKMARK);
}

IN_PROC_BROWSER_TEST_P(MigrationSingleClientTest,
                       AllTypesIndividuallyTriggerRefresh) {
  ASSERT_TRUE(SetupClients());
  RunSingleClientMigrationTest(GetPreferredDataTypesList(), TRIGGER_REFRESH);
}

IN_PROC_BROWSER_TEST_P(MigrationSingleClientTest, AllTypesAtOnce) {
  ASSERT_TRUE(SetupClients());
  RunSingleClientMigrationTest(MakeList(GetPreferredDataTypes()), MODIFY_PREF);
}

IN_PROC_BROWSER_TEST_P(MigrationSingleClientTest,
                       AllTypesAtOnceTriggerRefresh) {
  ASSERT_TRUE(SetupClients());
  RunSingleClientMigrationTest(MakeList(GetPreferredDataTypes()),
                               TRIGGER_REFRESH);
}

// All data types plus nigori.

IN_PROC_BROWSER_TEST_P(MigrationSingleClientTest,
                       AllTypesWithNigoriIndividually) {
  ASSERT_TRUE(SetupClients());
  MigrationList migration_list = GetPreferredDataTypesList();
  migration_list.push_front(MakeSet(syncer::NIGORI));
  RunSingleClientMigrationTest(migration_list, MODIFY_BOOKMARK);
}

IN_PROC_BROWSER_TEST_P(MigrationSingleClientTest, AllTypesWithNigoriAtOnce) {
  ASSERT_TRUE(SetupClients());
  syncer::DataTypeSet all_types = GetPreferredDataTypes();
  all_types.Put(syncer::NIGORI);
  RunSingleClientMigrationTest(MakeList(all_types), MODIFY_PREF);
}

IN_PROC_BROWSER_TEST_P(MigrationSingleClientTest,
                       PrefsMigrationDiscardMetadata) {
  ASSERT_TRUE(SetupSync());

  // 1. Create a pref before migration (toggles to true).
  preferences_helper::ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(FakeServerPrefMatchesValueChecker(syncer::PREFERENCES,
                                                prefs::kShowHomeButton, "true")
                  .Wait());

  // Locate our user preference entity before migration.
  const std::optional<sync_pb::SyncEntity> pref_before =
      FindPreferenceEntityByPrefName(GetFakeServer(), prefs::kShowHomeButton);
  ASSERT_TRUE(pref_before.has_value());
  const std::string id_before = pref_before->id_string();

  // 2. Trigger migration for Preferences on the server.
  TriggerMigrationDoneError({syncer::PREFERENCES});

  // 3. Trigger migration on the client and wait for it.
  TriggerSyncForDataTypes(0, {syncer::PREFERENCES});
  AwaitMigration({syncer::PREFERENCES});

  // Locate the migrated user preference entity after migration.
  const std::optional<sync_pb::SyncEntity> pref_after =
      FindPreferenceEntityByPrefName(GetFakeServer(), prefs::kShowHomeButton);
  ASSERT_TRUE(pref_after.has_value());
  const std::string id_after = pref_after->id_string();

  // Verify that the ID changed (migration happened) but data was preserved.
  EXPECT_NE(id_before, id_after)
      << "Preference entity ID did not change after migration!";
  ASSERT_EQ(pref_after->specifics().preference().value(), "true");

  // 4. Modify the pref again after migration (toggles to false).
  preferences_helper::ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(FakeServerPrefMatchesValueChecker(syncer::PREFERENCES,
                                                prefs::kShowHomeButton, "false")
                  .Wait());

  // 5. Verify the final committed update uses the migrated ID and mutates data.
  const std::optional<sync_pb::SyncEntity> pref_final =
      FindPreferenceEntityByPrefName(GetFakeServer(), prefs::kShowHomeButton);
  ASSERT_TRUE(pref_final.has_value());
  EXPECT_EQ(pref_final->id_string(), id_after);
  EXPECT_EQ(pref_final->specifics().preference().value(), "false")
      << "Client preference mutation was not committed successfully "
         "post-migration!";
}

IN_PROC_BROWSER_TEST_P(MigrationSingleClientTest,
                       BookmarksMigrationDiscardMetadata) {
  ASSERT_TRUE(SetupSync());

  // 1. Create a bookmark node before migration.
  const bookmarks::BookmarkNode* const node =
      AddURL(0, IndexedURLTitle(0), GURL(IndexedURL(0)), GetStoreType());
  ASSERT_TRUE(node);

  // Wait until it gets uploaded to the server.
  ASSERT_TRUE(ServerBookmarksEqualityChecker(
                  {{IndexedURLTitle(0), GURL(IndexedURL(0))}}, nullptr)
                  .Wait());

  // Locate our user bookmark entity before migration.
  const std::optional<sync_pb::SyncEntity> bookmark_before =
      FindBookmarkEntityByURL(GetFakeServer(), GURL(IndexedURL(0)));
  ASSERT_TRUE(bookmark_before.has_value());
  const std::string id_before = bookmark_before->id_string();

  // Verify initial data.
  ASSERT_EQ(bookmark_before->name(), base::UTF16ToUTF8(IndexedURLTitle(0)));

  // 2. Trigger migration for Bookmarks on the server.
  TriggerMigrationDoneError({syncer::BOOKMARKS});

  // 3. Trigger migration on the client and wait for it.
  TriggerSyncForDataTypes(0, {syncer::BOOKMARKS});
  AwaitMigration({syncer::BOOKMARKS});

  // Locate the migrated user bookmark entity after migration.
  const std::optional<sync_pb::SyncEntity> bookmark_after =
      FindBookmarkEntityByURL(GetFakeServer(), GURL(IndexedURL(0)));
  ASSERT_TRUE(bookmark_after.has_value());
  const std::string id_after = bookmark_after->id_string();

  // Verify that the ID changed (migration happened) but data was preserved.
  EXPECT_NE(id_before, id_after)
      << "Bookmark entity ID did not change after migration!";
  ASSERT_EQ(bookmark_after->name(), base::UTF16ToUTF8(IndexedURLTitle(0)));

  // 4. Modify the bookmark again after migration.
  const bookmarks::BookmarkNode* const migrated_node =
      GetUniqueNodeByURL(0, GURL(IndexedURL(0)));
  ASSERT_TRUE(migrated_node);
  SetTitle(0, migrated_node, u"New Title");

  // Wait until the modification gets uploaded to the server.
  ASSERT_TRUE(ServerBookmarksEqualityChecker(
                  {{u"New Title", GURL(IndexedURL(0))}}, nullptr)
                  .Wait());

  // 5. Verify the final committed update uses the migrated ID and mutates data.
  const std::optional<sync_pb::SyncEntity> bookmark_final =
      FindBookmarkEntityByURL(GetFakeServer(), GURL(IndexedURL(0)));
  ASSERT_TRUE(bookmark_final.has_value());
  EXPECT_EQ(bookmark_final->id_string(), id_after);
  EXPECT_EQ(bookmark_final->name(), "New Title")
      << "Client bookmark mutation was not committed successfully "
         "post-migration!";
}

class MigrationTwoClientTest : public MigrationTest {
 public:
  MigrationTwoClientTest() : MigrationTest(TWO_CLIENT) {}

  MigrationTwoClientTest(const MigrationTwoClientTest&) = delete;
  MigrationTwoClientTest& operator=(const MigrationTwoClientTest&) = delete;

  ~MigrationTwoClientTest() override = default;

  // Helper function that verifies that preferences sync still works.
  void VerifyPrefSync() {
    ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));
    ChangeBooleanPref(0, prefs::kShowHomeButton);
    ASSERT_TRUE(BooleanPrefMatchChecker(prefs::kShowHomeButton).Wait());
  }

  void RunTwoClientMigrationTest(const MigrationList& migration_list,
                                 TriggerMethod trigger_method) {
    ASSERT_TRUE(SetupSync());

    // Make sure pref sync works before running the migration test.
    VerifyPrefSync();

    RunMigrationTest(migration_list, trigger_method);

    // Make sure pref sync still works after running the migration
    // test.
    VerifyPrefSync();
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         MigrationTwoClientTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

// Easiest possible test of migration errors: triggers a server
// migration on one datatype, then modifies some other datatype.
IN_PROC_BROWSER_TEST_P(MigrationTwoClientTest, MigratePrefsThenModifyBookmark) {
  RunTwoClientMigrationTest(MakeList(syncer::PREFERENCES), MODIFY_BOOKMARK);
}

// Triggers a server migration on two datatypes, then makes a local
// modification to one of them.
IN_PROC_BROWSER_TEST_P(MigrationTwoClientTest,
                       MigratePrefsAndBookmarksThenModifyBookmark) {
  RunTwoClientMigrationTest(MakeList(syncer::PREFERENCES, syncer::BOOKMARKS),
                            MODIFY_BOOKMARK);
}

// Migrate every datatype in sequence; the catch being that the server
// will only tell the client about the migrations one at a time.
IN_PROC_BROWSER_TEST_P(MigrationTwoClientTest, MigrationHellWithoutNigori) {
  ASSERT_TRUE(SetupClients());
  MigrationList migration_list = GetPreferredDataTypesList();
  // Let the first nudge be a datatype that's neither prefs nor bookmarks.
  migration_list.push_front(MakeSet(syncer::THEMES));
  ASSERT_EQ(MakeSet(syncer::NIGORI), migration_list.back());
  migration_list.pop_back();
  RunTwoClientMigrationTest(migration_list, MODIFY_BOOKMARK);
}

IN_PROC_BROWSER_TEST_P(MigrationTwoClientTest, MigrationHellWithNigori) {
  ASSERT_TRUE(SetupClients());
  MigrationList migration_list = GetPreferredDataTypesList();
  // Let the first nudge be a datatype that's neither prefs nor bookmarks.
  migration_list.push_front(MakeSet(syncer::THEMES));
  ASSERT_EQ(MakeSet(syncer::NIGORI), migration_list.back());
  RunTwoClientMigrationTest(migration_list, MODIFY_BOOKMARK);
}

}  // namespace
