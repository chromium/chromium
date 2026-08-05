// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/tab_group_store/in_memory_tab_group_store.h"

#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace context_hub {

namespace {

using testing::_;
using testing::ElementsAre;
using testing::FieldsAre;
using testing::IsEmpty;
using testing::SizeIs;
using testing::UnorderedElementsAre;

class InMemoryTabGroupStoreTest : public testing::Test {
 protected:
  std::vector<TabGroupEntry> GetAllGroupsSync() {
    std::vector<TabGroupEntry> fetched_groups;
    store_.GetAllGroups(base::BindLambdaForTesting(
        [&](std::vector<TabGroupEntry> results) {
          fetched_groups = std::move(results);
        }));
    return fetched_groups;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  InMemoryTabGroupStore store_;
};

TEST_F(InMemoryTabGroupStoreTest, AddAndGetAllGroups) {
  std::vector<TabGroupEntry> groups;
  TabGroupEntry group1;
  group1.label = "Work";
  group1.tab_ids = {1, 2};
  groups.push_back(group1);

  TabGroupEntry group2;
  group2.id = "group_custom";
  group2.label = "Shopping";
  group2.tab_ids = {2, 3};  // Overlaps with group1 (tab 2)
  groups.push_back(group2);

  store_.AddAllGroups(groups, base::DoNothing());

  std::vector<TabGroupEntry> fetched_groups = GetAllGroupsSync();

  // Preserves group_custom ID and strips tab 2 from group1 via invariant
  EXPECT_THAT(
      fetched_groups,
      UnorderedElementsAre(
          FieldsAre("group_custom", "Shopping", ElementsAre(2, 3), _,
                    testing::Ne(base::Time()), testing::Ne(base::Time())),
          FieldsAre(testing::Ne(""), "Work", ElementsAre(1), _,
                    testing::Ne(base::Time()), testing::Ne(base::Time()))));
}

TEST_F(InMemoryTabGroupStoreTest, DeleteAllGroups) {
  std::vector<TabGroupEntry> groups;
  TabGroupEntry group;
  group.label = "Initial";
  group.tab_ids = {1};
  groups.push_back(group);

  store_.AddAllGroups(groups, base::DoNothing());
  EXPECT_THAT(GetAllGroupsSync(), SizeIs(1));

  store_.DeleteAllGroups(base::DoNothing());
  EXPECT_THAT(GetAllGroupsSync(), IsEmpty());

  groups.clear();
  group.label = "New Group";
  groups.push_back(group);
  store_.AddAllGroups(groups, base::DoNothing());

  EXPECT_THAT(
      GetAllGroupsSync(),
      ElementsAre(FieldsAre(testing::Ne(""), "New Group", ElementsAre(1), _,
                            testing::Ne(base::Time()),
                            testing::Ne(base::Time()))));
}

TEST_F(InMemoryTabGroupStoreTest, MaxCapacityEviction) {
  std::vector<TabGroupEntry> groups;
  for (size_t i = 0; i < 51; ++i) {
    TabGroupEntry group;
    group.label = base::StrCat({"Group ", base::NumberToString(i)});
    group.tab_ids = {static_cast<int64_t>(i)};
    groups.push_back(group);
  }

  store_.AddAllGroups(groups, base::DoNothing());

  std::vector<TabGroupEntry> fetched_groups = GetAllGroupsSync();

  // Capped at 50 max groups (Group 0 evicted)
  EXPECT_THAT(fetched_groups, SizeIs(50u));
  EXPECT_THAT(fetched_groups[0].label, testing::Eq("Group 1"));
  EXPECT_THAT(fetched_groups.back().label, testing::Eq("Group 50"));
}

TEST_F(InMemoryTabGroupStoreTest, AddOrUpdateGroup) {
  // Test addition with tab_id deduplication ({1, 1, 2} -> {1, 2})
  TabGroupEntry group;
  group.label = "Initial";
  group.tab_ids = {1, 1, 2};
  store_.AddOrUpdateGroup(group, base::DoNothing());

  EXPECT_THAT(
      GetAllGroupsSync(),
      ElementsAre(FieldsAre(testing::Ne(""), "Initial", ElementsAre(1, 2), _,
                            testing::Ne(base::Time()),
                            testing::Ne(base::Time()))));

  // Test update existing group preserves created_timestamp
  std::vector<TabGroupEntry> fetched_before = GetAllGroupsSync();
  ASSERT_THAT(fetched_before, SizeIs(1));
  base::Time original_created = fetched_before[0].created_timestamp;
  std::string assigned_id = fetched_before[0].id;

  TabGroupEntry updated;
  updated.id = assigned_id;
  updated.label = "Updated";
  updated.tab_ids = {1, 3};
  store_.AddOrUpdateGroup(updated, base::DoNothing());

  std::vector<TabGroupEntry> fetched_after = GetAllGroupsSync();
  ASSERT_THAT(fetched_after, SizeIs(1));
  EXPECT_EQ(fetched_after[0].created_timestamp, original_created);
  EXPECT_EQ(fetched_after[0].label, "Updated");

  // Test empty group rejection
  TabGroupEntry empty_group;
  empty_group.id = assigned_id;
  empty_group.label = "Empty";
  empty_group.tab_ids = {};
  store_.AddOrUpdateGroup(empty_group, base::DoNothing());

  EXPECT_THAT(GetAllGroupsSync(), IsEmpty());
}

TEST_F(InMemoryTabGroupStoreTest, DeleteGroup) {
  TabGroupEntry group;
  group.label = "To Delete";
  group.tab_ids = {1};
  store_.AddOrUpdateGroup(group, base::DoNothing());

  std::vector<TabGroupEntry> fetched = GetAllGroupsSync();
  ASSERT_THAT(fetched, SizeIs(1));
  std::string assigned_id = fetched[0].id;

  // Non-existent ID deletion is a no-op
  store_.DeleteGroup("non_existent_id", base::DoNothing());
  EXPECT_THAT(GetAllGroupsSync(), SizeIs(1));

  store_.DeleteGroup(assigned_id, base::DoNothing());
  EXPECT_THAT(GetAllGroupsSync(), IsEmpty());
}

TEST_F(InMemoryTabGroupStoreTest, PruneTabFromAllGroups) {
  TabGroupEntry group1;
  group1.label = "Group 1";
  group1.tab_ids = {1, 2};
  store_.AddOrUpdateGroup(group1, base::DoNothing());

  TabGroupEntry group2;
  group2.label = "Group 2";
  group2.tab_ids = {3};
  store_.AddOrUpdateGroup(group2, base::DoNothing());

  // Prune tab 3: removes tab 3 from Group 2 (Group 2 becomes empty and is pruned)
  store_.PruneTabFromAllGroups(3, base::DoNothing());

  EXPECT_THAT(
      GetAllGroupsSync(),
      ElementsAre(FieldsAre(testing::Ne(""), "Group 1", ElementsAre(1, 2), _,
                            testing::Ne(base::Time()),
                            testing::Ne(base::Time()))));
}

TEST_F(InMemoryTabGroupStoreTest, AddTabToGroup) {
  TabGroupEntry group;
  group.label = "Group";
  group.tab_ids = {1};
  store_.AddOrUpdateGroup(group, base::DoNothing());

  std::vector<TabGroupEntry> fetched = GetAllGroupsSync();
  ASSERT_THAT(fetched, SizeIs(1));
  std::string assigned_id = fetched[0].id;

  // Adding new tab
  store_.AddTabToGroup(assigned_id, 2, base::DoNothing());
  EXPECT_THAT(
      GetAllGroupsSync(),
      ElementsAre(FieldsAre(assigned_id, "Group", ElementsAre(1, 2), _,
                            testing::Ne(base::Time()),
                            testing::Ne(base::Time()))));

  // Adding duplicate tab (no-op)
  store_.AddTabToGroup(assigned_id, 1, base::DoNothing());
  EXPECT_THAT(
      GetAllGroupsSync(),
      ElementsAre(FieldsAre(assigned_id, "Group", ElementsAre(1, 2), _,
                            testing::Ne(base::Time()),
                            testing::Ne(base::Time()))));
}

TEST_F(InMemoryTabGroupStoreTest, UpdateGroupTimestampForTab) {
  TabGroupEntry group;
  group.label = "Group";
  group.tab_ids = {1, 2};
  base::Time past = base::Time::Now() - base::Seconds(100);
  group.last_accessed_timestamp = past;
  store_.AddOrUpdateGroup(group, base::DoNothing());

  base::Time recent = base::Time::Now();
  store_.UpdateGroupTimestampForTab(1, recent, base::DoNothing());

  std::vector<TabGroupEntry> fetched_groups = GetAllGroupsSync();
  ASSERT_THAT(fetched_groups, SizeIs(1));
  EXPECT_THAT(fetched_groups[0].last_accessed_timestamp, testing::Eq(recent));
}

TEST_F(InMemoryTabGroupStoreTest, EnforcesSingleGroupPerTab) {
  TabGroupEntry group1;
  group1.label = "Group 1";
  group1.tab_ids = {1, 2};
  store_.AddOrUpdateGroup(group1, base::DoNothing());

  TabGroupEntry group2;
  group2.label = "Group 2";
  group2.tab_ids = {3};
  store_.AddOrUpdateGroup(group2, base::DoNothing());

  std::vector<TabGroupEntry> fetched = GetAllGroupsSync();
  ASSERT_THAT(fetched, SizeIs(2));
  std::string group1_id = fetched[0].label == "Group 1" ? fetched[0].id : fetched[1].id;
  std::string group2_id = fetched[0].label == "Group 2" ? fetched[0].id : fetched[1].id;

  // Moving tab 1 from Group 1 to Group 2 via AddTabToGroup
  store_.AddTabToGroup(group2_id, 1, base::DoNothing());

  // Verify tab 1 is removed from Group 1, and now in Group 2
  EXPECT_THAT(
      GetAllGroupsSync(),
      UnorderedElementsAre(
          FieldsAre(group2_id, "Group 2", ElementsAre(3, 1), _,
                    testing::Ne(base::Time()), testing::Ne(base::Time())),
          FieldsAre(group1_id, "Group 1", ElementsAre(2), _,
                    testing::Ne(base::Time()), testing::Ne(base::Time()))));

  // Moving tab 2 to Group 2 via AddTabToGroup (Group 1 becomes empty and is pruned)
  store_.AddTabToGroup(group2_id, 2, base::DoNothing());

  // Verify Group 1 is auto-deleted since it has 0 tabs left
  EXPECT_THAT(
      GetAllGroupsSync(),
      ElementsAre(FieldsAre(group2_id, "Group 2", ElementsAre(3, 1, 2), _,
                            testing::Ne(base::Time()),
                            testing::Ne(base::Time()))));
}

}  // namespace
}  // namespace context_hub
