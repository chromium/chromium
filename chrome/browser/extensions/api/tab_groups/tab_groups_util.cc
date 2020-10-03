// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_groups/tab_groups_util.h"

#include "base/hash/hash.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/token.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/error_utils.h"

namespace extensions {
namespace tab_groups_util {

int GetGroupId(const tab_groups::TabGroupId& id) {
  uint32_t hash = base::PersistentHash(id.ToString());
  return std::abs(static_cast<int>(hash));
}

bool GetGroupById(int group_id,
                  content::BrowserContext* browser_context,
                  bool include_incognito,
                  Browser** browser,
                  tab_groups::TabGroupId* id,
                  const tab_groups::TabGroupVisualData** visual_data,
                  std::string* error) {
  if (group_id == -1)
    return false;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  Profile* incognito_profile =
      include_incognito && profile->HasPrimaryOTRProfile()
          ? profile->GetPrimaryOTRProfile()
          : nullptr;
  for (auto* target_browser : *BrowserList::GetInstance()) {
    if (target_browser->profile() == profile ||
        target_browser->profile() == incognito_profile) {
      TabStripModel* target_tab_strip = target_browser->tab_strip_model();
      for (tab_groups::TabGroupId target_group :
           target_tab_strip->group_model()->ListTabGroups()) {
        if (GetGroupId(target_group) == group_id) {
          if (browser)
            *browser = target_browser;
          if (id)
            *id = target_group;
          if (visual_data) {
            *visual_data = target_tab_strip->group_model()
                               ->GetTabGroup(target_group)
                               ->visual_data();
          }
          return true;
        }
      }
    }
  }

  *error =
      ErrorUtils::FormatErrorMessage(tab_groups_constants::kGroupNotFoundError,
                                     base::NumberToString(group_id));
  return false;
}

bool GetGroupById(int group_id,
                  content::BrowserContext* browser_context,
                  bool include_incognito,
                  tab_groups::TabGroupId* id,
                  std::string* error) {
  return GetGroupById(group_id, browser_context, include_incognito, nullptr, id,
                      nullptr, error);
}

}  // namespace tab_groups_util
}  // namespace extensions
