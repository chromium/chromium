// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/shared_tab_group_data_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
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
    feature_overrides_.InitWithFeatures(
        {data_sharing::features::kDataSharingFeature,
         tab_groups::kTabGroupsSaveUIUpdate, tab_groups::kTabGroupsSaveV2,
         tab_groups::kTabGroupSyncServiceDesktopMigration},
        {});
  }

  void SetUpOnMainThread() override {
    // Creates the fake server.
    SyncTest::SetUpOnMainThread();

    // Add the user to the collaboration before making any changes (to prevent
    // filtration of local entities on GetUpdates).
    GetFakeServer()->AddCollaboration(kCollaborationId);
  }
  ~TwoClientSharedTabGroupDataSyncTest() override = default;

  TabGroupSyncService* GetTabGroupSyncService(int profile_index) const {
    return TabGroupSyncServiceFactory::GetForProfile(GetProfile(profile_index));
  }

  // Returns both saved and shared tab groups.
  std::vector<SavedTabGroup> GetAllTabGroups(int profile_index) const {
    return GetTabGroupSyncService(profile_index)->GetAllGroups();
  }

  void AddTabGroup(int profile_index, SavedTabGroup group) {
    GetTabGroupSyncService(profile_index)->AddGroup(std::move(group));
  }

  void MoveTab(int profile_index,
               const SavedTabGroup& group,
               const SavedTabGroupTab& tab,
               size_t new_index) {
    GetTabGroupSyncService(profile_index)
        ->MoveTab(group.local_group_id().value(), tab.local_tab_id().value(),
                  new_index);
  }

  // Blocks the caller until both profiles have the same shared tab groups.
  bool WaitForMatchingModels() {
    return SharedTabGroupsMatchChecker(GetTabGroupSyncService(0),
                                       GetTabGroupSyncService(1))
        .Wait();
  }

  void FakeLocalOpeningOfGroup(SavedTabGroup& group_to_edit) {
#if BUILDFLAG(IS_ANDROID)
    group_to_edit.SetLocalGroupId(base::Token::CreateRandom());
#else
    group_to_edit.SetLocalGroupId(tab_groups::TabGroupId::GenerateNew());
#endif
  }
  void FakeLocalOpeningOfTab(SavedTabGroupTab& tab_to_edit) {
#if BUILDFLAG(IS_ANDROID)
    static int next_value = 1;
    tab_to_edit.SetLocalTabID(++next_value);
#else
    tab_to_edit.SetLocalTabID(base::Token::CreateRandom());
#endif
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
  FakeLocalOpeningOfGroup(group);

  group.SetCollaborationId(kCollaborationId);
  SavedTabGroupTab tab_1(GURL("http://google.com/1"), u"tab 1",
                         group.saved_guid(), /*position=*/std::nullopt);
  FakeLocalOpeningOfTab(tab_1);
  SavedTabGroupTab tab_2(GURL("http://google.com/2"), u"tab 2",
                         group.saved_guid(), /*position=*/std::nullopt);
  FakeLocalOpeningOfTab(tab_2);
  SavedTabGroupTab tab_3(GURL("http://google.com/3"), u"tab 3",
                         group.saved_guid(), /*position=*/std::nullopt);
  FakeLocalOpeningOfTab(tab_3);
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
