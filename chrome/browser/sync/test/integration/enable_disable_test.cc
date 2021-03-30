// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_user_settings_impl.h"
#include "components/sync/test/fake_server/bookmark_entity_builder.h"
#include "components/sync/test/fake_server/entity_builder_factory.h"
#include "content/public/test/browser_test.h"

namespace {

using syncer::ModelType;
using syncer::ModelTypeFromString;
using syncer::ModelTypeSet;
using syncer::ModelTypeToString;
using syncer::ProxyTypes;
using syncer::SyncUserSettings;
using syncer::UserSelectableType;
using syncer::UserSelectableTypeSet;

const char kSyncedBookmarkURL[] = "http://www.mybookmark.com";
// Non-utf8 string to make sure it gets handled well.
const char kTestServerChips[] = "\xed\xa0\x80\xed\xbf\xbf";

// Some types show up in multiple groups. This means that there are at least two
// user selectable groups that will cause these types to become enabled. This
// affects our tests because we cannot assume that before enabling a multi type
// it will be disabled, because the other selectable type(s) could already be
// enabling it. And vice versa for disabling.
ModelTypeSet MultiGroupTypes(const ModelTypeSet& registered_types) {
  ModelTypeSet seen;
  ModelTypeSet multi;
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    const ModelTypeSet grouped_types =
        syncer::SyncUserSettingsImpl::ResolvePreferredTypesForTesting({type});
    for (ModelType grouped_type : grouped_types) {
      if (seen.Has(grouped_type)) {
        multi.Put(grouped_type);
      } else {
        seen.Put(grouped_type);
      }
    }
  }
  multi.RetainAll(registered_types);
  return multi;
}

// This test enables and disables types and verifies the type is sufficiently
// affected by checking for existence of a root node.
class EnableDisableSingleClientTest : public SyncTest {
 public:
  EnableDisableSingleClientTest() : SyncTest(SINGLE_CLIENT) {}
  ~EnableDisableSingleClientTest() override = default;

  // Don't use self-notifications as they can trigger additional sync cycles.
  bool TestUsesSelfNotifications() override { return false; }

  bool ModelTypeExists(ModelType type) {
    base::RunLoop loop;
    std::unique_ptr<base::ListValue> all_nodes;
    GetSyncService(0)->GetAllNodesForDebugging(
        base::BindLambdaForTesting([&](std::unique_ptr<base::ListValue> nodes) {
          all_nodes = std::move(nodes);
          loop.Quit();
        }));
    loop.Run();
    // Look for the root node corresponding to |type|.
    for (const base::Value& value : all_nodes->GetList()) {
      DCHECK(value.is_dict());
      const base::Value* nodes = value.FindKey("nodes");
      DCHECK(nodes);
      DCHECK(nodes->is_list());
      // Ignore types that are empty, because we expect the root node.
      if (nodes->GetList().empty()) {
        continue;
      }
      const base::Value* model_type = value.FindKey("type");
      DCHECK(model_type);
      DCHECK(model_type->is_string());
      if (type == ModelTypeFromString(model_type->GetString())) {
        return true;
      }
    }
    return false;
  }

  void InjectSyncedBookmark() {
    fake_server::BookmarkEntityBuilder bookmark_builder =
        entity_builder_factory_.NewBookmarkEntityBuilder("My Bookmark");
    GetFakeServer()->InjectEntity(
        bookmark_builder.BuildBookmark(GURL(kSyncedBookmarkURL)));
  }

  int GetNumUpdatesDownloadedInLastCycle() {
    return GetSyncService(0)
        ->GetLastCycleSnapshotForDebugging()
        .model_neutral_state()
        .num_updates_downloaded_total;
  }

 protected:
  void SetupTest(bool all_types_enabled) {
    ASSERT_TRUE(SetupClients());
    if (all_types_enabled) {
      ASSERT_TRUE(GetClient(0)->SetupSync());
    } else {
      ASSERT_TRUE(
          GetClient(0)->SetupSyncNoWaitForCompletion(UserSelectableTypeSet()));
      ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
    }

    registered_data_types_ = GetSyncService(0)->GetRegisteredDataTypesForTest();
    multi_grouped_types_ = MultiGroupTypes(registered_data_types_);
    registered_selectable_types_ = GetRegisteredSelectableTypes(0);
  }

  ModelTypeSet ResolveGroup(UserSelectableType type) {
    ModelTypeSet grouped_types =
        syncer::SyncUserSettingsImpl::ResolvePreferredTypesForTesting({type});
    grouped_types.RetainAll(registered_data_types_);
    grouped_types.RemoveAll(ProxyTypes());
    return grouped_types;
  }

  ModelTypeSet WithoutMultiTypes(const ModelTypeSet& input) {
    return Difference(input, multi_grouped_types_);
  }

  ModelTypeSet registered_data_types_;
  ModelTypeSet multi_grouped_types_;
  UserSelectableTypeSet registered_selectable_types_;

 private:
  fake_server::EntityBuilderFactory entity_builder_factory_;

  DISALLOW_COPY_AND_ASSIGN(EnableDisableSingleClientTest);
};

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest, EnableOneAtATime) {
  // Setup sync with no enabled types.
  SetupTest(/*all_types_enabled=*/false);

  for (UserSelectableType type : registered_selectable_types_) {
    const ModelTypeSet grouped_types = ResolveGroup(type);
    for (ModelType single_grouped_type : WithoutMultiTypes(grouped_types)) {
      ASSERT_FALSE(ModelTypeExists(single_grouped_type))
          << " for " << GetUserSelectableTypeName(type);
    }

    base::HistogramTester histogram_tester;
    EXPECT_TRUE(GetClient(0)->EnableSyncForType(type));

    for (ModelType grouped_type : grouped_types) {
      EXPECT_TRUE(ModelTypeExists(grouped_type))
          << " for " << GetUserSelectableTypeName(type);

      if (syncer::CommitOnlyTypes().Has(grouped_type)) {
        EXPECT_EQ(0,
                  histogram_tester.GetBucketCount(
                      "Sync.PostedDataTypeGetUpdatesRequest",
                      static_cast<int>(ModelTypeHistogramValue(grouped_type))))
            << " for " << ModelTypeToString(grouped_type);
      } else {
        EXPECT_NE(0,
                  histogram_tester.GetBucketCount(
                      "Sync.PostedDataTypeGetUpdatesRequest",
                      static_cast<int>(ModelTypeHistogramValue(grouped_type))))
            << " for " << ModelTypeToString(grouped_type);
      }
    }
  }
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest, DisableOneAtATime) {
  // Setup sync with no disabled types.
  SetupTest(/*all_types_enabled=*/true);

  for (UserSelectableType type : registered_selectable_types_) {
    const ModelTypeSet grouped_types = ResolveGroup(type);
    for (ModelType grouped_type : grouped_types) {
      ASSERT_TRUE(ModelTypeExists(grouped_type))
          << " for " << GetUserSelectableTypeName(type);
    }

    EXPECT_TRUE(GetClient(0)->DisableSyncForType(type));

    for (ModelType single_grouped_type : WithoutMultiTypes(grouped_types)) {
      EXPECT_FALSE(ModelTypeExists(single_grouped_type))
          << " for " << GetUserSelectableTypeName(type);
    }
  }

  // Lastly make sure that all the multi grouped times are all gone, since we
  // did not check these after disabling inside the above loop.
  for (ModelType multi_grouped_type : multi_grouped_types_) {
    EXPECT_FALSE(ModelTypeExists(multi_grouped_type))
        << " for " << ModelTypeToString(multi_grouped_type);
  }
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest,
                       FastEnableDisableOneAtATime) {
  // Setup sync with no enabled types.
  SetupTest(/*all_types_enabled=*/false);

  for (UserSelectableType type : registered_selectable_types_) {
    const ModelTypeSet grouped_types = ResolveGroup(type);
    const ModelTypeSet single_grouped_types = WithoutMultiTypes(grouped_types);
    for (ModelType single_grouped_type : single_grouped_types) {
      ASSERT_FALSE(ModelTypeExists(single_grouped_type))
          << " for " << GetUserSelectableTypeName(type);
    }

    // Enable and then disable immediately afterwards, before the datatype has
    // had the chance to finish startup (which usually involves task posting).
    EXPECT_TRUE(GetClient(0)->EnableSyncForType(type));
    EXPECT_TRUE(GetClient(0)->DisableSyncForType(type));

    for (ModelType single_grouped_type : single_grouped_types) {
      EXPECT_FALSE(ModelTypeExists(single_grouped_type))
          << " for " << GetUserSelectableTypeName(type);
    }
  }

  // Lastly make sure that all the multi grouped times are all gone, since we
  // did not check these after disabling inside the above loop.
  for (ModelType multi_grouped_type : multi_grouped_types_) {
    EXPECT_FALSE(ModelTypeExists(multi_grouped_type))
        << " for " << ModelTypeToString(multi_grouped_type);
  }
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest,
                       FastDisableEnableOneAtATime) {
  // Setup sync with no disabled types.
  SetupTest(/*all_types_enabled=*/true);

  for (UserSelectableType type : registered_selectable_types_) {
    const ModelTypeSet grouped_types = ResolveGroup(type);
    for (ModelType grouped_type : grouped_types) {
      ASSERT_TRUE(ModelTypeExists(grouped_type))
          << " for " << GetUserSelectableTypeName(type);
    }

    // Disable and then reenable immediately afterwards, before the datatype has
    // had the chance to stop fully (which usually involves task posting).
    EXPECT_TRUE(GetClient(0)->DisableSyncForType(type));
    EXPECT_TRUE(GetClient(0)->EnableSyncForType(type));

    for (ModelType grouped_type : grouped_types) {
      EXPECT_TRUE(ModelTypeExists(grouped_type))
          << " for " << GetUserSelectableTypeName(type);
    }
  }
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest,
                       FastEnableDisableEnableOneAtATime) {
  // Setup sync with no enabled types.
  SetupTest(/*all_types_enabled=*/false);

  for (UserSelectableType type : registered_selectable_types_) {
    const ModelTypeSet single_grouped_types =
        WithoutMultiTypes(ResolveGroup(type));
    for (ModelType single_grouped_type : single_grouped_types) {
      ASSERT_FALSE(ModelTypeExists(single_grouped_type))
          << " for " << GetUserSelectableTypeName(type);
    }

    // Fast enable-disable-enable sequence, before the datatype has had the
    // chance to transition fully across states (usually involves task posting).
    EXPECT_TRUE(GetClient(0)->EnableSyncForType(type));
    EXPECT_TRUE(GetClient(0)->DisableSyncForType(type));
    EXPECT_TRUE(GetClient(0)->EnableSyncForType(type));

    for (ModelType single_grouped_type : single_grouped_types) {
      EXPECT_TRUE(ModelTypeExists(single_grouped_type))
          << " for " << GetUserSelectableTypeName(type);
    }
  }
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest, EnableDisable) {
  SetupTest(/*all_types_enabled=*/false);

  // Enable all, and then disable immediately afterwards, before datatypes
  // have had the chance to finish startup (which usually involves task
  // posting).
  GetClient(0)->EnableSyncForRegisteredDatatypes();
  GetClient(0)->DisableSyncForAllDatatypes();

  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    for (ModelType grouped_type : ResolveGroup(type)) {
      EXPECT_FALSE(ModelTypeExists(grouped_type))
          << " for " << GetUserSelectableTypeName(type);
    }
  }
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest, PRE_EnableAndRestart) {
  SetupTest(/*all_types_enabled=*/true);
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest, EnableAndRestart) {
  ASSERT_TRUE(SetupClients());

  EXPECT_TRUE(GetClient(0)->AwaitEngineInitialization());

  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    for (ModelType model_type : ResolveGroup(type)) {
      EXPECT_TRUE(ModelTypeExists(model_type))
          << " for " << ModelTypeToString(model_type);
    }
  }
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest, FastEnableDisableEnable) {
  SetupTest(/*all_types_enabled=*/false);

  // Enable all, and then disable+reenable immediately afterwards, before
  // datatypes have had the chance to finish startup (which usually involves
  // task posting).
  GetClient(0)->EnableSyncForRegisteredDatatypes();
  GetClient(0)->DisableSyncForAllDatatypes();
  GetClient(0)->EnableSyncForRegisteredDatatypes();

  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    for (ModelType model_type : ResolveGroup(type)) {
      EXPECT_TRUE(ModelTypeExists(model_type))
          << " for " << ModelTypeToString(model_type);
    }
  }
}

// This test makes sure that after a StopAndClear(), Sync data gets redownloaded
// when Sync is started again. This does not actually verify that the data is
// gone from disk (which seems infeasible); it's mostly here as a baseline for
// the following tests.
IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest,
                       RedownloadsAfterClearData) {
  ASSERT_TRUE(SetupClients());
  ASSERT_FALSE(bookmarks_helper::GetBookmarkModel(0)->IsBookmarked(
      GURL(kSyncedBookmarkURL)));

  // Create a bookmark on the server, then turn on Sync on the client.
  InjectSyncedBookmark();
  ASSERT_TRUE(GetClient(0)->SetupSync());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());

  // Make sure the bookmark got synced down.
  ASSERT_TRUE(bookmarks_helper::GetBookmarkModel(0)->IsBookmarked(
      GURL(kSyncedBookmarkURL)));
  // Note: The response may also contain permanent nodes, so we can't check the
  // exact count.
  const int initial_updates_downloaded = GetNumUpdatesDownloadedInLastCycle();
  ASSERT_GT(initial_updates_downloaded, 0);

  // Stop and restart Sync.
  GetClient(0)->StopSyncServiceAndClearData();
  GetClient(0)->StartSyncService();
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());

  // Everything should have been redownloaded.
  ASSERT_TRUE(bookmarks_helper::GetBookmarkModel(0)->IsBookmarked(
      GURL(kSyncedBookmarkURL)));
  EXPECT_EQ(GetNumUpdatesDownloadedInLastCycle(), initial_updates_downloaded);
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest,
                       DoesNotRedownloadAfterKeepData) {
  ASSERT_TRUE(SetupClients());
  ASSERT_FALSE(bookmarks_helper::GetBookmarkModel(0)->IsBookmarked(
      GURL(kSyncedBookmarkURL)));

  // Create a bookmark on the server, then turn on Sync on the client.
  InjectSyncedBookmark();
  ASSERT_TRUE(GetClient(0)->SetupSync());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());

  // Make sure the bookmark got synced down.
  ASSERT_TRUE(bookmarks_helper::GetBookmarkModel(0)->IsBookmarked(
      GURL(kSyncedBookmarkURL)));
  // Note: The response may also contain permanent nodes, so we can't check the
  // exact count.
  ASSERT_GT(GetNumUpdatesDownloadedInLastCycle(), 0);

  // Stop Sync and let it start up again in standalone transport mode.
  GetClient(0)->StopSyncServiceWithoutClearingData();
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureActive());

  // Now start full Sync again.
  base::HistogramTester histogram_tester;
  GetClient(0)->StartSyncService();
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());

  // The bookmark should still be there, *without* having been redownloaded.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(bookmarks_helper::GetBookmarkModel(0)->IsBookmarked(
      GURL(kSyncedBookmarkURL)));
  EXPECT_EQ(
      0, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.BOOKMARK",
                                         /*REMOTE_NON_INITIAL_UPDATE=*/4));
  EXPECT_EQ(
      0, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.BOOKMARK",
                                         /*REMOTE_INITIAL_UPDATE=*/5));
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest, ResetsPrefsIfClearData) {
  SetupTest(/*all_types_enabled=*/true);

  syncer::SyncTransportDataPrefs prefs(GetProfile(0)->GetPrefs());
  const std::string first_cache_guid = prefs.GetCacheGuid();
  ASSERT_NE("", first_cache_guid);

  GetClient(0)->StopSyncServiceAndClearData();
  // Sync should have restarted in transport mode, creating a new cache GUID.
  EXPECT_NE("", prefs.GetCacheGuid());
  EXPECT_NE(first_cache_guid, prefs.GetCacheGuid());
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest,
                       DoesNotClearPrefsWithKeepData) {
  SetupTest(/*all_types_enabled=*/true);

  syncer::SyncTransportDataPrefs prefs(GetProfile(0)->GetPrefs());
  const std::string cache_guid = prefs.GetCacheGuid();
  ASSERT_NE("", cache_guid);

  GetClient(0)->StopSyncServiceWithoutClearingData();
  EXPECT_EQ(cache_guid, prefs.GetCacheGuid());
}

class EnableDisableSingleClientSelfNotifyTest
    : public EnableDisableSingleClientTest {
 public:
  // UpdatedProgressMarkerChecker relies on the 'self-notify' feature.
  bool TestUsesSelfNotifications() override { return true; }

  sync_pb::ClientToServerMessage TriggerGetUpdatesCycleAndWait() {
    TriggerSyncForModelTypes(0, {syncer::BOOKMARKS});
    EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

    sync_pb::ClientToServerMessage message;
    EXPECT_TRUE(GetFakeServer()->GetLastGetUpdatesMessage(&message));
    return message;
  }
};

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientSelfNotifyTest,
                       PRE_ResendsBagOfChips) {
  sync_pb::ChipBag bag_of_chips;
  bag_of_chips.set_server_chips(kTestServerChips);
  ASSERT_FALSE(base::IsStringUTF8(bag_of_chips.SerializeAsString()));
  GetFakeServer()->SetBagOfChips(bag_of_chips);

  SetupTest(/*all_types_enabled=*/true);

  syncer::SyncTransportDataPrefs prefs(GetProfile(0)->GetPrefs());
  EXPECT_EQ(bag_of_chips.SerializeAsString(), prefs.GetBagOfChips());

  sync_pb::ClientToServerMessage message = TriggerGetUpdatesCycleAndWait();
  EXPECT_TRUE(message.has_bag_of_chips());
  EXPECT_EQ(kTestServerChips, message.bag_of_chips().server_chips());
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientSelfNotifyTest,
                       ResendsBagOfChips) {
  ASSERT_TRUE(SetupClients());
  syncer::SyncTransportDataPrefs prefs(GetProfile(0)->GetPrefs());
  ASSERT_NE("", prefs.GetBagOfChips());
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());

  sync_pb::ClientToServerMessage message = TriggerGetUpdatesCycleAndWait();
  EXPECT_TRUE(message.has_bag_of_chips());
  EXPECT_EQ(kTestServerChips, message.bag_of_chips().server_chips());
}

}  // namespace
