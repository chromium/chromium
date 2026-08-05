// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/tab_group_store/tab_group_entry_conversions.h"

#include "base/containers/span.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/sessions/core/session_id.h"
#include "components/tab_groups/tab_group_color.h"

namespace context_hub {

TabGroupEntry FromSavedTabGroup(const tab_groups::SavedTabGroup& group) {
  TabGroupEntry entry;
  entry.id = group.saved_guid().AsLowercaseString();
  entry.label = base::UTF16ToUTF8(group.title());
  entry.last_accessed_timestamp = group.update_time();
  entry.created_timestamp = group.creation_time();
  entry.tabs.reserve(group.saved_tabs().size());

  for (const tab_groups::SavedTabGroupTab& tab : group.saved_tabs()) {
    TabData tab_data;
    tab_data.id = tab.local_tab_id().value_or(SessionID::InvalidValue().id());
    tab_data.title = base::UTF16ToUTF8(tab.title());
    tab_data.url = tab.url();
    entry.tabs.push_back(std::move(tab_data));
  }
  return entry;
}

std::vector<TabGroupEntry> FromSavedTabGroups(
    base::span<const tab_groups::SavedTabGroup> groups) {
  std::vector<TabGroupEntry> entries;
  entries.reserve(groups.size());
  for (const tab_groups::SavedTabGroup& group : groups) {
    entries.push_back(FromSavedTabGroup(group));
  }
  return entries;
}

std::optional<tab_groups::SavedTabGroup> ToSavedTabGroup(
    const TabGroupEntry& entry) {
  if (entry.tabs.empty()) {
    return std::nullopt;
  }
  base::Uuid parsed_guid = base::Uuid::ParseLowercase(entry.id);
  base::Uuid group_guid =
      parsed_guid.is_valid() ? parsed_guid : base::Uuid::GenerateRandomV4();
  std::vector<tab_groups::SavedTabGroupTab> saved_tabs;
  saved_tabs.reserve(entry.tabs.size());
  size_t position = 0;
  for (const TabData& tab : entry.tabs) {
    saved_tabs.emplace_back(tab.url, base::UTF8ToUTF16(tab.title), group_guid,
                            position++);
  }
  // TODO(crbug.com/542736828): Check if Context Hub tab groups can be saved
  // without pinning to the tab bar.
  return tab_groups::SavedTabGroup(base::UTF8ToUTF16(entry.label),
                                   tab_groups::TabGroupColorId::kBlue,
                                   std::move(saved_tabs),
                                   /*position=*/std::nullopt, group_guid);
}

}  // namespace context_hub
