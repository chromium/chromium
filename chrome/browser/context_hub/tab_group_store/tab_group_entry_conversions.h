// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_TAB_GROUP_STORE_TAB_GROUP_ENTRY_CONVERSIONS_H_
#define CHROME_BROWSER_CONTEXT_HUB_TAB_GROUP_STORE_TAB_GROUP_ENTRY_CONVERSIONS_H_

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "chrome/browser/context_hub/tab_group_store/tab_group_entry.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"

namespace context_hub {

// Converts a native confirmed Chrome SavedTabGroup into a Context Hub
// TabGroupEntry.
TabGroupEntry FromSavedTabGroup(const tab_groups::SavedTabGroup& group);

// Converts a collection of native confirmed Chrome SavedTabGroups into
// TabGroupEntries.
std::vector<TabGroupEntry> FromSavedTabGroups(
    base::span<const tab_groups::SavedTabGroup> groups);

// Converts an unconfirmed Context Hub TabGroupEntry into a native confirmed
// Chrome SavedTabGroup.
std::optional<tab_groups::SavedTabGroup> ToSavedTabGroup(
    const TabGroupEntry& entry);

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_TAB_GROUP_STORE_TAB_GROUP_ENTRY_CONVERSIONS_H_
