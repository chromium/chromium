// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/test/chrome_app_list_test_support.h"

#include <memory>
#include <string>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "chrome/browser/browser_process.h"
#include "components/crx_file/id_util.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace test {

namespace {

// Create the icon image for the app-item with |id|.
// TODO(mukai): consolidate the implementation with
// ash/app_list/model/app_list_test_model.cc.
gfx::ImageSkia CreateImageSkia(int id) {
  const int size =
      ash::SharedAppListConfig::instance().default_grid_icon_dimension();
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size);
  bitmap.eraseARGB(255, 255 * ((id >> 2) % 2), 255 * ((id >> 1) % 2),
                   255 * (id % 2));
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

}  // namespace

AppListModelUpdater* GetModelUpdater(AppListClientImpl* client) {
  return app_list::AppListSyncableServiceFactory::GetForProfile(
             client->GetCurrentAppListProfile())
      ->GetModelUpdater();
}

AppListClientImpl* GetAppListClient() {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  client->UpdateProfile();
  return client;
}

void PopulateDummyAppListItems(int n) {
  AppListClientImpl* client = GetAppListClient();
  Profile* profile = client->GetCurrentAppListProfile();
  AppListModelUpdater* model_updater = GetModelUpdater(client);

  // Calculate `last_position` among the existing app list items.
  std::vector<const ChromeAppListItem*> existing_items =
      model_updater->GetItems();
  syncer::StringOrdinal last_position =
      syncer::StringOrdinal::CreateInitialOrdinal();
  for (const auto* item : existing_items) {
    if (item->position().GreaterThan(last_position))
      last_position = item->position();
  }

  syncer::StringOrdinal new_item_position = last_position.CreateAfter();
  for (int i = 0; i < n; ++i) {
    const std::string app_name = base::StringPrintf("app %d", i);
    const std::string app_id = crx_file::id_util::GenerateId(app_name);
    auto item =
        std::make_unique<ChromeAppListItem>(profile, app_id, model_updater);
    auto metadata = std::make_unique<ash::AppListItemMetadata>();
    metadata->id = app_id;
    metadata->name = app_name;
    metadata->icon = CreateImageSkia(i);
    metadata->position = new_item_position;
    new_item_position = new_item_position.CreateAfter();
    item->SetMetadata(std::move(metadata));
    model_updater->AddItem(std::move(item));
  }
  // Wait for the AddItem mojo calls to be handled by Ash. Note that
  // FlushMojoForTesting() isn't working well here.
  // TODO(mukai): remove this once we eliminate the mojo for app-list.
  base::RunLoop().RunUntilIdle();
}

}  // namespace test
