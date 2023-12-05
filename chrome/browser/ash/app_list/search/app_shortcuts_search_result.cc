// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/app_shortcuts_search_result.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"

namespace app_list {

AppShortcutSearchResult::AppShortcutSearchResult(const std::string& id,
                                                 const std::u16string& title,
                                                 Profile* profile,
                                                 double relevance)
    : profile_(profile) {
  DCHECK(profile);

  set_id(id);
  SetTitle(title);
  SetCategory(Category::kAppShortcuts);
  SetResultType(ash::AppListSearchResultType::kAppShortcutV2);
  SetDisplayType(ash::SearchResultDisplayType::kList);
  SetMetricsType(ash::APP_SHORTCUTS_V2);
  set_relevance(relevance);
}

AppShortcutSearchResult::~AppShortcutSearchResult() = default;

void AppShortcutSearchResult::Open(int event_flags) {
  // TODO(owenzhang): Implement it.
}

}  // namespace app_list
