// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/app_list_test_api.h"

#include <string>
#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/shell.h"
#include "ui/views/view_model.h"

namespace ash {

AppListTestApi::AppListTestApi() = default;
AppListTestApi::~AppListTestApi() = default;

bool AppListTestApi::HasApp(const std::string& app_id) {
  return Shell::Get()->app_list_controller()->GetModel()->FindItem(app_id);
}

std::vector<std::string> AppListTestApi::GetTopLevelViewIdList() {
  std::vector<std::string> id_list;
  AppListView* app_list_view =
      Shell::Get()->app_list_controller()->presenter()->GetView();
  AppListView::TestApi test_api(app_list_view);
  auto* view_model = test_api.GetRootAppsGridView()->view_model();
  for (int i = 0; i < view_model->view_size(); ++i) {
    AppListItem* app_list_item = view_model->view_at(i)->item();
    if (app_list_item) {
      id_list.push_back(app_list_item->id());
    }
  }
  return id_list;
}

std::string AppListTestApi::CreateFolderWithApps(
    const std::vector<std::string>& apps) {
  // Only create a folder if there are two or more apps.
  DCHECK_GE(apps.size(), 2u);

  AppListModel* model = Shell::Get()->app_list_controller()->GetModel();
  // Create a folder using the first two apps, and add the others to the folder
  // iteratively.
  std::string folder_id = model->MergeItems(apps[0], apps[1]);
  // Return early if MergeItems failed.
  if (folder_id.empty())
    return "";
  for (size_t i = 2; i < apps.size(); ++i)
    model->MergeItems(folder_id, apps[i]);
  return folder_id;
}

std::string AppListTestApi::GetFolderId(const std::string& app_id) {
  return Shell::Get()
      ->app_list_controller()
      ->GetModel()
      ->FindItem(app_id)
      ->folder_id();
}

std::vector<std::string> AppListTestApi::GetAppIdsInFolder(
    const std::string& folder_id) {
  AppListItem* folder_item =
      Shell::Get()->app_list_controller()->GetModel()->FindItem(folder_id);
  DCHECK(folder_item->is_folder());
  AppListItemList* folder_list =
      static_cast<AppListFolderItem*>(folder_item)->item_list();
  std::vector<std::string> id_list;
  for (size_t i = 0; i < folder_list->item_count(); ++i)
    id_list.push_back(folder_list->item_at(i)->id());
  return id_list;
}

void AppListTestApi::MoveItemToPosition(const std::string& item_id,
                                        const size_t to_index) {
  AppListItem* app_item =
      Shell::Get()->app_list_controller()->GetModel()->FindItem(item_id);
  const std::string folder_id = app_item->folder_id();

  AppListItemList* item_list;
  std::vector<std::string> top_level_id_list = GetTopLevelViewIdList();
  // The app should be either at the top level or in a folder.
  if (folder_id.empty()) {
    // The app is at the top level.
    item_list =
        Shell::Get()->app_list_controller()->GetModel()->top_level_item_list();
  } else {
    // The app is in the folder with |folder_id|.
    item_list = Shell::Get()
                    ->app_list_controller()
                    ->GetModel()
                    ->FindFolderItem(folder_id)
                    ->item_list();
  }
  size_t from_index = 0;
  item_list->FindItemIndex(item_id, &from_index);
  item_list->MoveItem(from_index, to_index);
}

}  // namespace ash
