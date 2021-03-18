// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_APP_LIST_TEST_API_H_
#define ASH_PUBLIC_CPP_TEST_APP_LIST_TEST_API_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"

namespace ash {

// Accesses ash data for app list view testing.
class ASH_EXPORT AppListTestApi {
 public:
  AppListTestApi();
  ~AppListTestApi();
  AppListTestApi(const AppListTestApi& other) = delete;
  AppListTestApi& operator=(const AppListTestApi& other) = delete;

  // Returns whether there is an item for |app_id|.
  bool HasApp(const std::string& app_id);

  // Returns ids of the items in top level app list view.
  std::vector<std::string> GetTopLevelViewIdList();

  // Creates a folder and moves all the apps in |apps| into that folder. Returns
  // the created folder id or empty string on error. Note that |apps| should
  // contains at least two items.
  std::string CreateFolderWithApps(const std::vector<std::string>& apps);

  // Returns the folder id that the app with |app_id| belongs to. Returns empty
  // string if the app is not in a folder.
  std::string GetFolderId(const std::string& app_id);

  // Returns IDs of all apps that belong to the folder with |folder_id|.
  std::vector<std::string> GetAppIdsInFolder(const std::string& folder_id);

  // Moves an item to position |to_index| within the item's item list. The item
  // can be a folder.
  void MoveItemToPosition(const std::string& item_id, const size_t to_index);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_APP_LIST_MODEL_API_H_
