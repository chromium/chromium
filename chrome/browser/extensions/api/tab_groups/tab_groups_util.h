// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TAB_GROUPS_TAB_GROUPS_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_TAB_GROUPS_TAB_GROUPS_UTIL_H_

#include <memory>
#include <string>

#include "components/tab_groups/tab_group_color.h"

class Browser;

namespace content {
class BrowserContext;
}

namespace tab_groups {
class TabGroupId;
class TabGroupVisualData;
}  // namespace tab_groups

namespace extensions {

// Provides various utility functions that help manipulate tab groups.
namespace tab_groups_util {

// Gets the extensions-specific Group ID.
int GetGroupId(const tab_groups::TabGroupId& id);

// Gets the metadata for the group with ID |group_id|. Sets the |error| if not
// found. |browser|, |id|, or |visual_data| may be nullptr and will not be set
// within the function if so.
bool GetGroupById(int group_id,
                  content::BrowserContext* browser_context,
                  bool include_incognito,
                  Browser** browser,
                  tab_groups::TabGroupId* id,
                  const tab_groups::TabGroupVisualData** visual_data,
                  std::string* error);
bool GetGroupById(int group_id,
                  content::BrowserContext* browser_context,
                  bool include_incognito,
                  tab_groups::TabGroupId* id,
                  std::string* error);

}  // namespace tab_groups_util
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TAB_GROUPS_TAB_GROUPS_UTIL_H_
