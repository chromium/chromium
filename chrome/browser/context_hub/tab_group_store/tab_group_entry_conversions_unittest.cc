// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/tab_group_store/tab_group_entry_conversions.h"

#include <optional>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/sessions/core/session_id.h"
#include "components/tab_groups/tab_group_color.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace context_hub {

namespace {

TEST(TabGroupEntryConversionsTest, FromSavedTabGroup_BasicConversion) {
  tab_groups::SavedTabGroup group(u"Work",
                                  tab_groups::TabGroupColorId::kBlue, {},
                                  /*position=*/std::nullopt);
  tab_groups::SavedTabGroupTab tab1(GURL("https://example.com/1"), u"Tab 1",
                                    group.saved_guid(), /*position=*/0);
  tab_groups::SavedTabGroupTab tab2(GURL("https://example.com/2"), u"Tab 2",
                                    group.saved_guid(), /*position=*/1);
  group.AddTabLocally(tab1);
  group.AddTabLocally(tab2);

  TabGroupEntry entry = FromSavedTabGroup(group);

  EXPECT_EQ(entry.id, group.saved_guid().AsLowercaseString());
  EXPECT_EQ(entry.label, "Work");
  EXPECT_EQ(entry.last_accessed_timestamp, group.update_time());
  EXPECT_EQ(entry.created_timestamp, group.creation_time());
  ASSERT_EQ(entry.tabs.size(), 2u);
  EXPECT_EQ(entry.tabs[0].id, SessionID::InvalidValue().id());
  EXPECT_EQ(entry.tabs[0].title, "Tab 1");
  EXPECT_EQ(entry.tabs[0].url, GURL("https://example.com/1"));
  EXPECT_EQ(entry.tabs[1].id, SessionID::InvalidValue().id());
  EXPECT_EQ(entry.tabs[1].title, "Tab 2");
  EXPECT_EQ(entry.tabs[1].url, GURL("https://example.com/2"));
}

TEST(TabGroupEntryConversionsTest, FromSavedTabGroups_EmptyInput) {
  std::vector<tab_groups::SavedTabGroup> empty_groups;
  std::vector<TabGroupEntry> empty_entries = FromSavedTabGroups(empty_groups);
  EXPECT_TRUE(empty_entries.empty());
}

TEST(TabGroupEntryConversionsTest, FromSavedTabGroups_MultipleGroups) {
  tab_groups::SavedTabGroup group1(u"Group 1",
                                   tab_groups::TabGroupColorId::kBlue, {},
                                   /*position=*/std::nullopt);
  tab_groups::SavedTabGroup group2(u"Group 2",
                                   tab_groups::TabGroupColorId::kRed, {},
                                   /*position=*/std::nullopt);
  std::vector<tab_groups::SavedTabGroup> groups = {group1, group2};

  std::vector<TabGroupEntry> entries = FromSavedTabGroups(groups);
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries[0].label, "Group 1");
  EXPECT_EQ(entries[1].label, "Group 2");
}

TEST(TabGroupEntryConversionsTest, ToSavedTabGroup_ReturnsNulloptWhenTabsEmpty) {
  TabGroupEntry entry;
  entry.id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry.label = "Empty Group";
  entry.tabs = {};

  std::optional<tab_groups::SavedTabGroup> confirmed_group =
      ToSavedTabGroup(entry);
  EXPECT_FALSE(confirmed_group.has_value());
}

TEST(TabGroupEntryConversionsTest,
     ToSavedTabGroup_GeneratesRandomUuidForInvalidStringId) {
  TabGroupEntry entry;
  entry.id = "invalid_id";
  entry.label = "Shopping";
  TabData tab;
  tab.title = "Item";
  tab.url = GURL("https://store.com");
  entry.tabs.push_back(tab);

  std::optional<tab_groups::SavedTabGroup> confirmed_group =
      ToSavedTabGroup(entry);
  ASSERT_TRUE(confirmed_group.has_value());
  EXPECT_TRUE(confirmed_group->saved_guid().is_valid());
  EXPECT_NE(confirmed_group->saved_guid().AsLowercaseString(), "invalid_id");
  EXPECT_EQ(base::UTF16ToUTF8(confirmed_group->title()), "Shopping");
  ASSERT_EQ(confirmed_group->saved_tabs().size(), 1u);
  EXPECT_EQ(base::UTF16ToUTF8(confirmed_group->saved_tabs()[0].title()),
            "Item");
  EXPECT_EQ(confirmed_group->saved_tabs()[0].url(), GURL("https://store.com"));
}

TEST(TabGroupEntryConversionsTest,
     ToSavedTabGroup_PreservesValidUuidForConfirmedGroup) {
  base::Uuid original_uuid = base::Uuid::GenerateRandomV4();
  TabGroupEntry entry;
  entry.id = original_uuid.AsLowercaseString();
  entry.label = "Confirmed Group";
  TabData tab;
  tab.title = "Page";
  tab.url = GURL("https://page.com");
  entry.tabs.push_back(tab);

  std::optional<tab_groups::SavedTabGroup> confirmed_group =
      ToSavedTabGroup(entry);
  ASSERT_TRUE(confirmed_group.has_value());
  EXPECT_EQ(confirmed_group->saved_guid(), original_uuid);
  EXPECT_EQ(base::UTF16ToUTF8(confirmed_group->title()), "Confirmed Group");
}

}  // namespace
}  // namespace context_hub
