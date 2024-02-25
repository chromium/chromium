// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/app_list_test_model.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_controller.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {
namespace test {

gfx::ImageSkia CreateImageSkia(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseARGB(255, 0, 255, 0);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

// static
const char AppListTestModel::kItemType[] = "TestItem";

// AppListTestModel::AppListTestItem

AppListTestModel::AppListTestItem::AppListTestItem(const std::string& id,
                                                   AppListTestModel* model)
    : AppListItem(id), model_(model) {
  const int icon_dimension =
      SharedAppListConfig::instance().default_grid_icon_dimension();
  SetDefaultIconAndColor(CreateImageSkia(icon_dimension, icon_dimension),
                         IconColor(), /*is_placeholder_icon=*/false);
}

AppListTestModel::AppListTestItem::~AppListTestItem() = default;

void AppListTestModel::AppListTestItem::Activate(int event_flags) {
  model_->ItemActivated(this);
}

std::unique_ptr<ui::SimpleMenuModel>
AppListTestModel::AppListTestItem::CreateContextMenuModel() {
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(
      nullptr /*no SimpleMenuModelDelegate for tests*/);
  menu_model->AddItem(0, u"0");
  menu_model->AddItem(1, u"1");
  return menu_model;
}

const char* AppListTestModel::AppListTestItem::GetItemType() const {
  return AppListTestModel::kItemType;
}

void AppListTestModel::AppListTestItem::SetPosition(
    const syncer::StringOrdinal& new_position) {
  set_position(new_position);
}

// AppListTestModel

AppListTestModel::AppListTestModel()
    : AppListModel(/*app_list_model_delegate=*/this) {}

AppListTestModel::~AppListTestModel() = default;

AppListItem* AppListTestModel::AddItem(AppListItem* item) {
  return AppListModel::AddItem(base::WrapUnique(item));
}

void AppListTestModel::RequestPositionUpdate(
    std::string id,
    const syncer::StringOrdinal& new_position,
    RequestPositionUpdateReason reason) {
  // Copy the logic of `ChromeAppListModelUpdater::HandleSetPosition()`.
  auto metadata = FindItem(id)->CloneMetadata();
  metadata->position = new_position;
  SetItemMetadata(id, std::move(metadata));
}

void AppListTestModel::RequestMoveItemToFolder(std::string id,
                                               const std::string& folder_id) {
  // Copy the logic of `ChromeAppListModelUpdater::RequestMoveItemToFolder()`.
  AppListFolderItem* dest_folder = FindFolderItem(folder_id);
  DCHECK(dest_folder);
  const syncer::StringOrdinal target_position =
      dest_folder->item_list()->CreatePositionBefore(syncer::StringOrdinal());

  auto metadata = FindItem(id)->CloneMetadata();
  metadata->folder_id = folder_id;
  metadata->position = target_position;
  SetItemMetadata(id, std::move(metadata));
}

void AppListTestModel::RequestMoveItemToRoot(
    std::string id,
    syncer::StringOrdinal target_position) {
  // Copy the logic of `ChromeAppListModelUpdater::RequestMoveItemToRoot()`.
  auto metadata = FindItem(id)->CloneMetadata();
  metadata->folder_id = "";
  metadata->position = target_position;
  SetItemMetadata(id, std::move(metadata));
}

std::string AppListTestModel::RequestFolderCreation(
    std::string merge_target_id,
    std::string item_to_merge_id) {
  auto target_item_metadata = FindItem(merge_target_id)->CloneMetadata();
  const syncer::StringOrdinal target_item_position =
      target_item_metadata->position;

  const std::string folder_id = AppListFolderItem::GenerateId();
  auto folder = std::make_unique<AppListFolderItem>(
      folder_id, /*app_list_model_delegate=*/this);
  auto folder_metadata = folder->CloneMetadata();
  folder_metadata->position = target_item_position;
  folder->SetMetadata(std::move(folder_metadata));
  AddItem(folder.release());

  target_item_metadata->folder_id = folder_id;
  SetItemMetadata(merge_target_id, std::move(target_item_metadata));

  auto item_to_merge_metadata = FindItem(item_to_merge_id)->CloneMetadata();
  item_to_merge_metadata->position = target_item_position.CreateAfter();
  item_to_merge_metadata->folder_id = folder_id;
  SetItemMetadata(item_to_merge_id, std::move(item_to_merge_metadata));

  return folder_id;
}

void AppListTestModel::RequestFolderRename(std::string id,
                                           const std::string& new_name) {
  auto metadata = FindItem(id)->CloneMetadata();
  metadata->name = new_name;
  SetItemMetadata(id, std::move(metadata));
}

void AppListTestModel::RequestAppListSort(AppListSortOrder order) {
  requested_sort_order_ = order;
}

void AppListTestModel::RequestAppListSortRevert() {
  requested_sort_order_.reset();
}

void AppListTestModel::RequestCommitTemporarySortOrder() {
  // Committing the temporary sort order should not introduce item reorder so
  // reset the sort order without reorder animation.
  AppListController::Get()->UpdateAppListWithNewTemporarySortOrder(
      /*new_order=*/std::nullopt, /*animate=*/false, base::NullCallback());
}

AppListItem* AppListTestModel::AddItemToFolder(AppListItem* item,
                                               const std::string& folder_id) {
  return AppListModel::AddItemToFolder(base::WrapUnique(item), folder_id);
}

void AppListTestModel::MoveItemToFolder(AppListItem* item,
                                        const std::string& folder_id) {
  AppListModel::MoveItemToFolder(item, folder_id);
}

// static
std::string AppListTestModel::GetItemName(int id) {
  return base::StringPrintf("Item %d", id);
}

void AppListTestModel::PopulateApps(int n) {
  for (int i = 0; i < n; ++i) {
    CreateAndAddItem(GetItemName(naming_index_++));
  }
}

AppListFolderItem* AppListTestModel::CreateAndPopulateFolderWithApps(int n) {
  DCHECK_GT(n, 1);
  AppListTestItem* item = CreateAndAddItem(GetItemName(naming_index_++));
  std::string merged_item_id = item->id();
  for (int i = 1; i < n; ++i) {
    AppListTestItem* new_item = CreateAndAddItem(GetItemName(naming_index_++));
    merged_item_id = AppListModel::MergeItems(merged_item_id, new_item->id());
  }
  AppListItem* merged_item = FindItem(merged_item_id);
  DCHECK(merged_item->GetItemType() == AppListFolderItem::kItemType);
  return static_cast<AppListFolderItem*>(merged_item);
}

AppListFolderItem* AppListTestModel::CreateAndAddOemFolder() {
  AppListFolderItem* folder = new AppListFolderItem(
      ash::kOemFolderId, /*app_list_model_delegate=*/this);
  return static_cast<AppListFolderItem*>(AddItem(folder));
}

AppListFolderItem* AppListTestModel::CreateSingleItemFolder(
    const std::string& folder_id,
    const std::string& item_id) {
  AppListTestItem* item = CreateItem(item_id);
  AddItemToFolder(item, folder_id);
  AppListItem* folder_item = FindItem(folder_id);
  DCHECK(folder_item->GetItemType() == AppListFolderItem::kItemType);
  return static_cast<AppListFolderItem*>(folder_item);
}

AppListFolderItem* AppListTestModel::CreateSingleWebAppShortcutItemFolder(
    const std::string& folder_id,
    const std::string& item_id) {
  AppListTestItem* item = CreateWebAppShortcutItem(item_id);
  AddItemToFolder(item, folder_id);
  AppListItem* folder_item = FindItem(folder_id);
  DCHECK(folder_item->GetItemType() == AppListFolderItem::kItemType);
  return static_cast<AppListFolderItem*>(folder_item);
}

void AppListTestModel::PopulateAppWithId(int id) {
  CreateAndAddItem(GetItemName(id));
}

std::string AppListTestModel::GetModelContent() {
  std::vector<std::string> ids;
  ids.reserve(top_level_item_list()->item_count());

  for (size_t i = 0; i < top_level_item_list()->item_count(); ++i)
    ids.push_back(top_level_item_list()->item_at(i)->id());
  return base::JoinString(ids, ",");
}

syncer::StringOrdinal AppListTestModel::CalculatePosition() {
  size_t nitems = top_level_item_list()->item_count();
  syncer::StringOrdinal position;
  if (nitems == 0) {
    position = syncer::StringOrdinal::CreateInitialOrdinal();
  } else {
    position =
        top_level_item_list()->item_at(nitems - 1)->position().CreateAfter();
  }
  return position;
}

AppListTestModel::AppListTestItem* AppListTestModel::CreateItem(
    const std::string& id) {
  AppListTestItem* item = new AppListTestItem(id, this);
  item->SetPosition(CalculatePosition());
  SetItemName(item, id);
  return item;
}

AppListTestModel::AppListTestItem* AppListTestModel::CreateWebAppShortcutItem(
    const std::string& id) {
  AppListTestItem* test_item = new AppListTestItem(id, this);
  const int badge_icon_dimension = 48;
  const gfx::ImageSkia fake_badge_icon =
      CreateImageSkia(badge_icon_dimension, badge_icon_dimension);
  test_item->UpdateAppHostBadgeForTesting(fake_badge_icon);
  test_item->SetPosition(CalculatePosition());
  SetItemName(test_item, id);

  return test_item;
}

AppListTestModel::AppListTestItem* AppListTestModel::CreateAndAddItem(
    const std::string& id) {
  std::unique_ptr<AppListTestItem> test_item(CreateItem(id));
  AppListItem* item = AppListModel::AddItem(std::move(test_item));
  return static_cast<AppListTestItem*>(item);
}

AppListTestModel::AppListTestItem* AppListTestModel::CreateAndAddPromiseItem(
    const std::string& id) {
  std::unique_ptr<AppListTestItem> test_item(CreateItem(id));
  test_item->UpdateAppStatusForTesting(AppStatus::kPending);
  const int icon_dimension = 48;
  const gfx::ImageSkia fake_promise_icon =
      gfx::ImageSkiaOperations::CreateImageWithRoundRectClip(
          icon_dimension, CreateImageSkia(icon_dimension, icon_dimension));
  AppListItem* item = AppListModel::AddItem(std::move(test_item));
  return static_cast<AppListTestItem*>(item);
}

AppListTestModel::AppListTestItem*
AppListTestModel::CreateAndAddWebAppShortcutItemWithHostBadge(
    const std::string& id) {
  std::unique_ptr<AppListTestItem> test_item(CreateItem(id));
  const int badge_icon_dimension = 48;
  const gfx::ImageSkia fake_badge_icon =
      gfx::test::CreateImageSkia(badge_icon_dimension, SK_ColorCYAN);
  test_item->UpdateAppHostBadgeForTesting(fake_badge_icon);
  AppListItem* item = AppListModel::AddItem(std::move(test_item));
  SetItemName(item, id);
  return static_cast<AppListTestItem*>(item);
}

void AppListTestModel::ItemActivated(AppListTestItem* item) {
  last_activated_ = item;
  ++activate_count_;
}

}  // namespace test
}  // namespace ash
