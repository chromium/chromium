// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/shared_tab_group_data_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/saved_tab_groups/saved_tab_group_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace tab_groups {
namespace {

using testing::ElementsAre;
using testing::SizeIs;

constexpr char kCollaborationId[] = "collaboration";

class TwoClientSharedTabGroupDataSyncTest : public SyncTest {
 public:
  TwoClientSharedTabGroupDataSyncTest() : SyncTest(TWO_CLIENT) {
    feature_overrides_.InitAndEnableFeature(
        data_sharing::features::kDataSharingFeature);
  }

  void SetUpOnMainThread() override {
    // Creates the fake server.
    SyncTest::SetUpOnMainThread();

    // Add the user to the collaboration before making any changes (to prevent
    // filtration of local entities on GetUpdates).
    GetFakeServer()->AddCollaboration(kCollaborationId);
  }
  ~TwoClientSharedTabGroupDataSyncTest() override = default;

  SavedTabGroupModel* GetSavedTabGroupModel(int profile_index) const {
    return SavedTabGroupServiceFactory::GetForProfile(GetProfile(profile_index))
        ->model();
  }

  // Returns both saved and shared tab groups.
  std::vector<SavedTabGroup> GetAllTabGroups(int profile_index) const {
    return GetSavedTabGroupModel(profile_index)->saved_tab_groups();
  }

  void AddTabGroup(int profile_index, SavedTabGroup group) {
    GetSavedTabGroupModel(profile_index)->Add(std::move(group));
  }

  void MoveTab(int profile_index,
               const SavedTabGroup& group,
               const SavedTabGroupTab& tab,
               size_t new_index) {
    GetSavedTabGroupModel(profile_index)
        ->MoveTabInGroupTo(group.saved_guid(), tab.saved_tab_guid(), new_index);
  }

  // Blocks the caller until both profiles have the same shared tab groups.
  bool WaitForMatchingModels() {
    return SharedTabGroupsMatchChecker(*GetSavedTabGroupModel(0),
                                       *GetSavedTabGroupModel(1))
        .Wait();
  }

 private:
  base::test::ScopedFeatureList feature_overrides_;
};

IN_PROC_BROWSER_TEST_F(TwoClientSharedTabGroupDataSyncTest,
                       ShouldSyncGroupWithTabs) {
  ASSERT_TRUE(SetupSync());

  SavedTabGroup group(u"title", TabGroupColorId::kBlue,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(kCollaborationId);
  SavedTabGroupTab tab_1(GURL("http://google.com/1"), u"tab 1",
                         group.saved_guid(), /*position=*/std::nullopt);
  SavedTabGroupTab tab_2(GURL("http://google.com/2"), u"tab 2",
                         group.saved_guid(), /*position=*/std::nullopt);
  group.AddTabLocally(tab_1);
  group.AddTabLocally(tab_2);
  AddTabGroup(0, group);

  ASSERT_TRUE(WaitForMatchingModels());

  ASSERT_THAT(GetAllTabGroups(1),
              ElementsAre(HasSharedGroupMetadata(
                  "title", TabGroupColorId::kBlue, kCollaborationId)));
  EXPECT_THAT(GetAllTabGroups(1).front().saved_tabs(),
              ElementsAre(HasTabMetadata("tab 1", "http://google.com/1"),
                          HasTabMetadata("tab 2", "http://google.com/2")));
}

IN_PROC_BROWSER_TEST_F(TwoClientSharedTabGroupDataSyncTest,
                       ShouldSyncTabPositions) {
  ASSERT_TRUE(SetupSync());

  SavedTabGroup group(u"title", TabGroupColorId::kBlue,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(kCollaborationId);
  SavedTabGroupTab tab_1(GURL("http://google.com/1"), u"tab 1",
                         group.saved_guid(), /*position=*/std::nullopt);
  SavedTabGroupTab tab_2(GURL("http://google.com/2"), u"tab 2",
                         group.saved_guid(), /*position=*/std::nullopt);
  SavedTabGroupTab tab_3(GURL("http://google.com/3"), u"tab 3",
                         group.saved_guid(), /*position=*/std::nullopt);
  group.AddTabLocally(tab_1);
  group.AddTabLocally(tab_2);
  group.AddTabLocally(tab_3);
  AddTabGroup(0, group);

  ASSERT_TRUE(WaitForMatchingModels());
  ASSERT_THAT(GetAllTabGroups(1),
              ElementsAre(HasSharedGroupMetadata(
                  "title", TabGroupColorId::kBlue, kCollaborationId)));
  ASSERT_THAT(GetAllTabGroups(1).front().saved_tabs(),
              ElementsAre(HasTabMetadata("tab 1", "http://google.com/1"),
                          HasTabMetadata("tab 2", "http://google.com/2"),
                          HasTabMetadata("tab 3", "http://google.com/3")));

  // Move tab to the end.
  MoveTab(0, group, tab_1, /*new_index=*/2);
  ASSERT_TRUE(WaitForMatchingModels());
  ASSERT_THAT(GetAllTabGroups(1), SizeIs(1));
  EXPECT_THAT(GetAllTabGroups(1).front().saved_tabs(),
              ElementsAre(HasTabMetadata("tab 2", "http://google.com/2"),
                          HasTabMetadata("tab 3", "http://google.com/3"),
                          HasTabMetadata("tab 1", "http://google.com/1")));

  // Move tab in the middle.
  MoveTab(0, group, tab_1, /*new_index=*/1);
  ASSERT_TRUE(WaitForMatchingModels());
  ASSERT_THAT(GetAllTabGroups(1), SizeIs(1));
  EXPECT_THAT(GetAllTabGroups(1).front().saved_tabs(),
              ElementsAre(HasTabMetadata("tab 2", "http://google.com/2"),
                          HasTabMetadata("tab 1", "http://google.com/1"),
                          HasTabMetadata("tab 3", "http://google.com/3")));

  // Move tab to the beginning.
  MoveTab(0, group, tab_1, /*new_index=*/0);
  WaitForMatchingModels();
  ASSERT_THAT(GetAllTabGroups(1), SizeIs(1));
  EXPECT_THAT(GetAllTabGroups(1).front().saved_tabs(),
              ElementsAre(HasTabMetadata("tab 1", "http://google.com/1"),
                          HasTabMetadata("tab 2", "http://google.com/2"),
                          HasTabMetadata("tab 3", "http://google.com/3")));
}

}  // namespace
}  // namespace tab_groups
