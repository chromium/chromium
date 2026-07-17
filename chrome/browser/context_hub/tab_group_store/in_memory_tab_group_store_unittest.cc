// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/tab_group_store/in_memory_tab_group_store.h"

#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace context_hub {

namespace {

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
  group2.label = "Shopping";
  group2.tab_ids = {3, 4};
  groups.push_back(group2);

  store_.AddAllGroups(groups, base::DoNothing());

  std::vector<TabGroupEntry> fetched_groups = GetAllGroupsSync();

  ASSERT_EQ(fetched_groups.size(), 2u);
  EXPECT_EQ(fetched_groups[0].id, "group_1");
  EXPECT_EQ(fetched_groups[0].label, "Work");
  EXPECT_EQ(fetched_groups[0].tab_ids, (std::vector<int64_t>{1, 2}));

  EXPECT_EQ(fetched_groups[1].id, "group_2");
  EXPECT_EQ(fetched_groups[1].label, "Shopping");
  EXPECT_EQ(fetched_groups[1].tab_ids, (std::vector<int64_t>{3, 4}));
}

TEST_F(InMemoryTabGroupStoreTest, AddAllGroups_PreservesExistingID) {
  std::vector<TabGroupEntry> groups;
  TabGroupEntry group;
  group.id = "group_custom";
  group.label = "Custom Group";
  group.tab_ids = {10, 20};
  groups.push_back(group);

  store_.AddAllGroups(groups, base::DoNothing());

  std::vector<TabGroupEntry> fetched_groups = GetAllGroupsSync();

  ASSERT_EQ(fetched_groups.size(), 1u);
  EXPECT_EQ(fetched_groups[0].id, "group_custom");
  EXPECT_EQ(fetched_groups[0].label, "Custom Group");
}

TEST_F(InMemoryTabGroupStoreTest, DeleteAllGroupsResetsCounter) {
  std::vector<TabGroupEntry> groups;
  TabGroupEntry group;
  group.label = "Initial";
  group.tab_ids = {1};
  groups.push_back(group);

  store_.AddAllGroups(groups, base::DoNothing());

  std::vector<TabGroupEntry> fetched_groups = GetAllGroupsSync();
  ASSERT_EQ(fetched_groups.size(), 1u);
  EXPECT_EQ(fetched_groups[0].id, "group_1");

  store_.DeleteAllGroups(base::DoNothing());

  fetched_groups = GetAllGroupsSync();
  EXPECT_TRUE(fetched_groups.empty());

  // Verify counter reset to 1
  groups.clear();
  group.label = "New Group";
  groups.push_back(group);
  store_.AddAllGroups(groups, base::DoNothing());

  fetched_groups = GetAllGroupsSync();
  ASSERT_EQ(fetched_groups.size(), 1u);
  EXPECT_EQ(fetched_groups[0].id, "group_1");
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

  // Capped at 50 max groups (group_1 evicted)
  EXPECT_EQ(fetched_groups.size(), 50u);
  EXPECT_EQ(fetched_groups[0].id, "group_2");
  EXPECT_EQ(fetched_groups.back().id, "group_51");
}
}  // namespace
}  // namespace context_hub
