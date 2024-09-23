// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_list_model_updater.h"

#include <algorithm>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"

namespace {

int g_next_unique_model_id = ash::kAppListProfileIdStartFrom;

}  // namespace

AppListModelUpdater::AppListModelUpdater()
    : model_id_(g_next_unique_model_id++) {}

AppListModelUpdater::~AppListModelUpdater() = default;

syncer::StringOrdinal AppListModelUpdater::GetFirstAvailablePosition() const {
  const std::vector<ChromeAppListItem*>& top_level_items = GetTopLevelItems();
  auto last_item =
      std::max_element(top_level_items.begin(), top_level_items.end(),
                       [](ChromeAppListItem* const& item1,
                          ChromeAppListItem* const& item2) -> bool {
                         return item1->position().LessThan(item2->position());
                       });

  if (last_item == top_level_items.end())
    return syncer::StringOrdinal::CreateInitialOrdinal();

  return (*last_item)->position().CreateAfter();
}

std::vector<raw_ptr<ChromeSearchResult, VectorExperimental>>
AppListModelUpdater::GetPublishedSearchResultsForTest() {
  return std::vector<raw_ptr<ChromeSearchResult, VectorExperimental>>();
}

// static
syncer::StringOrdinal AppListModelUpdater::GetPositionBeforeFirstItemInternal(
    const std::vector<ChromeAppListItem*>& top_level_items) {
  auto iter =
      std::min_element(top_level_items.begin(), top_level_items.end(),
                       [](ChromeAppListItem* const& item1,
                          ChromeAppListItem* const& item2) -> bool {
                         return item1->position().LessThan(item2->position());
                       });

  if (iter == top_level_items.end())
    return syncer::StringOrdinal::CreateInitialOrdinal();

  return (*iter)->position().CreateBefore();
}

bool AppListModelUpdater::ModelHasBeenReorderedInThisSession() {
  return false;
}
