// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/saved_tab_groups_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/fake_server.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SingleClientSavedTabGroupsSyncTest : public SyncTest {
 public:
  SingleClientSavedTabGroupsSyncTest() : SyncTest(SINGLE_CLIENT) {
    features_.InitWithFeatures(
        {features::kTabGroupsSave, features::kTabGroupsSaveSyncIntegration},
        /*disabled_features=*/{});
  }
  ~SingleClientSavedTabGroupsSyncTest() override = default;
  SingleClientSavedTabGroupsSyncTest(
      const SingleClientSavedTabGroupsSyncTest&) = delete;
  SingleClientSavedTabGroupsSyncTest& operator=(
      const SingleClientSavedTabGroupsSyncTest&) = delete;

  void AddDataToFakeServer(const sync_pb::SavedTabGroupSpecifics& specifics) {
    sync_pb::EntitySpecifics group_entity_specifics;
    sync_pb::SavedTabGroupSpecifics* group_specifics =
        group_entity_specifics.mutable_saved_tab_group();
    group_specifics->CopyFrom(specifics);

    std::string client_tag = group_specifics->guid();
    int64_t creation_time =
        group_specifics->creation_time_windows_epoch_micros();
    int64_t update_time = group_specifics->update_time_windows_epoch_micros();

    fake_server_->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            "non_unique_name", client_tag, group_entity_specifics,
            /*creation_time=*/creation_time,
            /*last_modified_time=*/update_time));
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Save a group with two tabs and validate they are added to the model.
IN_PROC_BROWSER_TEST_F(SingleClientSavedTabGroupsSyncTest,
                       DownloadsGroupAndTabs) {
  SavedTabGroup group1(u"Group 1", tab_groups::TabGroupColorId::kGrey, {});
  SavedTabGroupTab tab1(GURL("about:blank"), u"about:blank",
                        group1.saved_guid());
  SavedTabGroupTab tab2(GURL("about:blank"), u"about:blank",
                        group1.saved_guid());

  // Add a group with two tabs to sync.
  AddDataToFakeServer(*group1.ToSpecifics());
  AddDataToFakeServer(*tab1.ToSpecifics());
  AddDataToFakeServer(*tab2.ToSpecifics());

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::SAVED_TAB_GROUP));

  SavedTabGroupKeyedService* const service =
      SavedTabGroupServiceFactory::GetForProfile(GetProfile(0));

  // Verify they are added to the model.
  EXPECT_TRUE(saved_tab_groups_helper::SavedTabOrGroupExistsChecker(
                  service, group1.saved_guid())
                  .Wait());

  EXPECT_TRUE(saved_tab_groups_helper::SavedTabOrGroupExistsChecker(
                  service, tab1.saved_tab_guid())
                  .Wait());

  EXPECT_TRUE(saved_tab_groups_helper::SavedTabOrGroupExistsChecker(
                  service, tab2.saved_tab_guid())
                  .Wait());
}

// Save a group with no tabs and validate it is added to the model.
IN_PROC_BROWSER_TEST_F(SingleClientSavedTabGroupsSyncTest,
                       DownloadsGroupWithNoTabs) {
  SavedTabGroup group1(u"Group 1", tab_groups::TabGroupColorId::kGrey, {});
  SavedTabGroupTab tab1(GURL("about:blank"), u"about:blank",
                        group1.saved_guid());

  // Add a group with no tabs from sync.
  AddDataToFakeServer(*group1.ToSpecifics());

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::SAVED_TAB_GROUP));

  SavedTabGroupKeyedService* const service =
      SavedTabGroupServiceFactory::GetForProfile(GetProfile(0));

  // Verify the group is added to the model but not the tab.
  EXPECT_TRUE(saved_tab_groups_helper::SavedTabOrGroupExistsChecker(
                  service, group1.saved_guid())
                  .Wait());

  EXPECT_TRUE(service->model()->Contains(group1.saved_guid()));
  EXPECT_TRUE(service->model()->Get(group1.saved_guid())->saved_tabs().empty());
}

// Save a tab with no group and validate it is added to the model.
IN_PROC_BROWSER_TEST_F(SingleClientSavedTabGroupsSyncTest,
                       DownloadsTabWithNoGroup) {
  SavedTabGroup group1(u"Group 1", tab_groups::TabGroupColorId::kGrey, {});
  SavedTabGroupTab tab1(GURL("about:blank"), u"about:blank",
                        group1.saved_guid());

  // Add a group with no tabs from sync.
  AddDataToFakeServer(*tab1.ToSpecifics());

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::SAVED_TAB_GROUP));

  SavedTabGroupKeyedService* const service =
      SavedTabGroupServiceFactory::GetForProfile(GetProfile(0));

  // TODO(crbug/1445672): Verify that the orphaned tab was exists but isn't
  // linked to any group.

  // Verify adding the corresponding group adds the orphaned tab to the model.
  AddDataToFakeServer(*group1.ToSpecifics());

  EXPECT_TRUE(saved_tab_groups_helper::SavedTabOrGroupExistsChecker(
                  service, group1.saved_guid())
                  .Wait());

  EXPECT_TRUE(saved_tab_groups_helper::SavedTabOrGroupExistsChecker(
                  service, tab1.saved_tab_guid())
                  .Wait());
}

// TODO(crbug/1445146): Implement remaining integration tests.
}  // namespace
