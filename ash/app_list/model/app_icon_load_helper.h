// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_APP_ICON_LOAD_HELPER_H_
#define ASH_APP_LIST_MODEL_APP_ICON_LOAD_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_item_list.h"
#include "ash/app_list/model/app_list_item_list_observer.h"
#include "ash/app_list/model/app_list_item_observer.h"
#include "ash/app_list/model/app_list_model_export.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"

namespace ash {

// Helper to trigger icon load of AppListeItem when its icon version number
// changes. It could be created for either a single AppListItem, or an
// AppListItemList for a folder where it watch for the first
// FolderImage::kNumFolderTopItems (currently 4) AppListItem in the list.
class APP_LIST_MODEL_EXPORT AppIconLoadHelper : public AppListItemObserver {
 public:
  using IconLoadCallback = base::RepeatingCallback<void(const std::string&)>;
  AppIconLoadHelper(AppListItem* app_item, IconLoadCallback icon_load_callback);
  AppIconLoadHelper(AppListItemList* list, IconLoadCallback icon_load_callback);
  AppIconLoadHelper(const AppIconLoadHelper&) = delete;
  AppIconLoadHelper& operator=(const AppIconLoadHelper&) = delete;
  ~AppIconLoadHelper() override;

 private:
  class AppItemHelper;
  class AppItemListHelper;

  std::unique_ptr<AppItemHelper> item_helper_;
  std::unique_ptr<AppItemListHelper> list_helper_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_MODEL_APP_ICON_LOAD_HELPER_H_
