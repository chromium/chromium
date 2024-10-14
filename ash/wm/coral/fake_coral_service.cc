// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/coral/fake_coral_service.h"

namespace ash {

void FakeCoralService::Group(coral::mojom::GroupRequestPtr request,
                             GroupCallback callback) {
  const int total_num = request->entities.size();
  const int min_group_size = request->clustering_options->min_items_in_cluster;
  const int max_group_size = request->clustering_options->max_items_in_cluster;
  CHECK_GE(total_num, min_group_size);

  // Get tab and app items from the request.
  std::vector<GURL> tab_urls;
  std::vector<std::string> app_ids;
  for (const coral::mojom::EntityPtr& entity : request->entities) {
    if (entity->is_tab()) {
      tab_urls.emplace_back(entity->get_tab()->url);
    } else {
      app_ids.emplace_back(entity->get_app()->id);
    }
  }

  // Create fake groups from the items in the request.
  auto response = coral::mojom::GroupResponse::New();
  auto create_group = [&](const std::string& name, size_t tab_start,
                          size_t tab_num, size_t app_start,
                          size_t app_num) -> coral::mojom::GroupPtr {
    auto group = coral::mojom::Group::New();
    group->title = name;
    for (size_t i = tab_start; i < tab_start + tab_num; i++) {
      group->entities.push_back(
          coral::mojom::EntityKey::NewTabUrl(tab_urls[i]));
    }
    for (size_t i = app_start; i < app_start + app_num; i++) {
      group->entities.push_back(coral::mojom::EntityKey::NewAppId(app_ids[i]));
    }
    return group;
  };

  // Try evenly split the tabs and apps from the request into two groups, but
  // the number of items in each group should be valid.
  const int group_size_1 =
      std::clamp(total_num / 2, min_group_size, max_group_size);
  // Assign the tabs and apps to the group in proportion to their total num;
  const int tab_total = tab_urls.size();
  const int tab_num_1 = group_size_1 * tab_total / total_num;
  const int app_num_1 = group_size_1 - tab_num_1;
  if (group_size_1) {
    response->groups.push_back(create_group(/*name=*/"Fake Group 1",
                                            /*tab_start=*/0, tab_num_1,
                                            /*app_start=*/0, app_num_1));
  }

  // Try to generate another group.
  const int residual = total_num - group_size_1;
  const int group_size_2 =
      residual < min_group_size ? 0 : std::min(residual, max_group_size);
  const int tab_num_2 = group_size_2 * (tab_total - tab_num_1) / residual;
  const int app_num_2 = group_size_2 - tab_num_2;
  if (group_size_2) {
    response->groups.push_back(create_group(/*name=*/"Fake Group 2",
                                            /*tab_start=*/tab_num_1, tab_num_2,
                                            /*app_start=*/app_num_1,
                                            app_num_2));
  }

  auto group_result =
      coral::mojom::GroupResult::NewResponse(std::move(response));
  std::move(callback).Run(std::move(group_result));
}

void FakeCoralService::CacheEmbeddings(
    coral::mojom::CacheEmbeddingsRequestPtr request,
    CacheEmbeddingsCallback callback) {
  std::move(callback).Run(coral::mojom::CacheEmbeddingsResult::NewResponse(
      coral::mojom::CacheEmbeddingsResponse::New()));
}

}  // namespace ash
