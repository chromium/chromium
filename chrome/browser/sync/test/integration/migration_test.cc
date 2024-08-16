// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/migration_waiter.h"
#include "chrome/browser/sync/test/integration/migration_watcher.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/pref_names.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/browser_test.h"

using bookmarks_helper::AddURL;
using bookmarks_helper::IndexedURL;
using bookmarks_helper::IndexedURLTitle;

using preferences_helper::BooleanPrefMatches;
using preferences_helper::ChangeBooleanPref;

namespace {

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

class MigrationTest : public SyncTest {
 public:
  explicit MigrationTest(TestType test_type) : SyncTest(test_type) {}

  MigrationTest(const MigrationTest&) = delete;
  MigrationTest& operator=(const MigrationTest&) = delete;

  ~MigrationTest() override = default;

  enum TriggerMethod { MODIFY_PREF, MODIFY_BOOKMARK, TRIGGER_REFRESH };

  // Initialize all MigrationWatchers. This helps ensure that all migration
  // events are captured, even if they were to occur before a test calls
  // AwaitMigration for a specific profile.
  void Initialize() {
    for (int i = 0; i < num_clients(); ++i) {
      migration_watchers_.push_back(
          std::make_unique<MigrationWatcher>(GetClient(i)));
    }
  }

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
        ASSERT_TRUE(AddURL(0, IndexedURLTitle(0), GURL(IndexedURL(0))));
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
      ASSERT_TRUE(
          MigrationWaiter(migrate_types, migration_watchers_[i].get()).Wait());
    }
  }

  // Makes sure migration works with the given migration list and
  // trigger method.
  void RunMigrationTest(const MigrationList& migration_list,
                        TriggerMethod trigger_method) {
    // Make sure migration hasn't been triggered prematurely.
    for (int i = 0; i < num_clients(); ++i) {
      ASSERT_TRUE(migration_watchers_[i]->GetMigratedTypes().empty());
    }

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
  // Used to keep track of the migration progress for each sync client.
  std::vector<std::unique_ptr<MigrationWatcher>> migration_watchers_;
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
    Initialize();
    RunMigrationTest(migration_list, trigger_method);
  }
};

// The simplest possible migration tests -- a single data type.

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, PrefsOnlyModifyPref) {
  RunSingleClientMigrationTest(MakeList(syncer::PREFERENCES), MODIFY_PREF);
}

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, PrefsOnlyModifyBookmark) {
  RunSingleClientMigrationTest(MakeList(syncer::PREFERENCES), MODIFY_BOOKMARK);
}

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, PrefsOnlyTriggerRefresh) {
  RunSingleClientMigrationTest(MakeList(syncer::PREFERENCES), TRIGGER_REFRESH);
}

// Nigori is handled specially, so we test that separately.

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, NigoriOnly) {
  RunSingleClientMigrationTest(MakeList(syncer::PREFERENCES), TRIGGER_REFRESH);
}

// A little more complicated -- two data types.

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, BookmarksPrefsIndividually) {
  RunSingleClientMigrationTest(MakeList(syncer::BOOKMARKS, syncer::PREFERENCES),
                               MODIFY_PREF);
}

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, BookmarksPrefsBoth) {
  RunSingleClientMigrationTest(
      MakeList(MakeSet(syncer::BOOKMARKS, syncer::PREFERENCES)),
      MODIFY_BOOKMARK);
}

// Two data types with one being nigori.

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, PrefsNigoriIndividiaully) {
  RunSingleClientMigrationTest(MakeList(syncer::PREFERENCES, syncer::NIGORI),
                               TRIGGER_REFRESH);
}

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, PrefsNigoriBoth) {
  RunSingleClientMigrationTest(
      MakeList(MakeSet(syncer::PREFERENCES, syncer::NIGORI)), MODIFY_PREF);
}

// The whole shebang -- all data types.
IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, AllTypesIndividually) {
  ASSERT_TRUE(SetupClients());
  RunSingleClientMigrationTest(GetPreferredDataTypesList(), MODIFY_BOOKMARK);
}

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest,
                       AllTypesIndividuallyTriggerRefresh) {
  ASSERT_TRUE(SetupClients());
  RunSingleClientMigrationTest(GetPreferredDataTypesList(), TRIGGER_REFRESH);
}

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, AllTypesAtOnce) {
  ASSERT_TRUE(SetupClients());
  RunSingleClientMigrationTest(MakeList(GetPreferredDataTypes()), MODIFY_PREF);
}

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest,
                       AllTypesAtOnceTriggerRefresh) {
  ASSERT_TRUE(SetupClients());
  RunSingleClientMigrationTest(MakeList(GetPreferredDataTypes()),
                               TRIGGER_REFRESH);
}

// All data types plus nigori.

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest,
                       AllTypesWithNigoriIndividually) {
  ASSERT_TRUE(SetupClients());
  MigrationList migration_list = GetPreferredDataTypesList();
  migration_list.push_front(MakeSet(syncer::NIGORI));
  RunSingleClientMigrationTest(migration_list, MODIFY_BOOKMARK);
}

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, AllTypesWithNigoriAtOnce) {
  ASSERT_TRUE(SetupClients());
  syncer::DataTypeSet all_types = GetPreferredDataTypes();
  all_types.Put(syncer::NIGORI);
  RunSingleClientMigrationTest(MakeList(all_types), MODIFY_PREF);
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
    Initialize();

    // Make sure pref sync works before running the migration test.
    VerifyPrefSync();

    RunMigrationTest(migration_list, trigger_method);

    // Make sure pref sync still works after running the migration
    // test.
    VerifyPrefSync();
  }
};

// Easiest possible test of migration errors: triggers a server
// migration on one datatype, then modifies some other datatype.
IN_PROC_BROWSER_TEST_F(MigrationTwoClientTest, MigratePrefsThenModifyBookmark) {
  RunTwoClientMigrationTest(MakeList(syncer::PREFERENCES), MODIFY_BOOKMARK);
}

// Triggers a server migration on two datatypes, then makes a local
// modification to one of them.
IN_PROC_BROWSER_TEST_F(MigrationTwoClientTest,
                       MigratePrefsAndBookmarksThenModifyBookmark) {
  RunTwoClientMigrationTest(MakeList(syncer::PREFERENCES, syncer::BOOKMARKS),
                            MODIFY_BOOKMARK);
}

// Migrate every datatype in sequence; the catch being that the server
// will only tell the client about the migrations one at a time.
IN_PROC_BROWSER_TEST_F(MigrationTwoClientTest, MigrationHellWithoutNigori) {
  ASSERT_TRUE(SetupClients());
  MigrationList migration_list = GetPreferredDataTypesList();
  // Let the first nudge be a datatype that's neither prefs nor bookmarks.
  migration_list.push_front(MakeSet(syncer::THEMES));
  ASSERT_EQ(MakeSet(syncer::NIGORI), migration_list.back());
  migration_list.pop_back();
  RunTwoClientMigrationTest(migration_list, MODIFY_BOOKMARK);
}

IN_PROC_BROWSER_TEST_F(MigrationTwoClientTest, MigrationHellWithNigori) {
  ASSERT_TRUE(SetupClients());
  MigrationList migration_list = GetPreferredDataTypesList();
  // Let the first nudge be a datatype that's neither prefs nor bookmarks.
  migration_list.push_front(MakeSet(syncer::THEMES));
  ASSERT_EQ(MakeSet(syncer::NIGORI), migration_list.back());
  RunTwoClientMigrationTest(migration_list, MODIFY_BOOKMARK);
}

}  // namespace
