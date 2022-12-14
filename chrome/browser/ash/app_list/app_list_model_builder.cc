// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_list_model_builder.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"

////////////////////////////////////////////////////////////////////////////////
// AppListModelBuilder::ScopedAppPositionInitCallbackForTest

AppListModelBuilder::ScopedAppPositionInitCallbackForTest::
    ScopedAppPositionInitCallbackForTest(AppListModelBuilder* builder,
                                         AppPositionInitCallback callback)
    : builder_(builder), callback_(callback) {
  DCHECK(!builder->position_setter_for_test_);
  builder->position_setter_for_test_ = &callback_;
}

AppListModelBuilder::ScopedAppPositionInitCallbackForTest::
    ~ScopedAppPositionInitCallbackForTest() {
  DCHECK_EQ(builder_->position_setter_for_test_, &callback_);
  builder_->position_setter_for_test_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// AppListModelBuilder

AppListModelBuilder::AppListModelBuilder(AppListControllerDelegate* controller,
                                         const char* item_type)
    : controller_(controller), item_type_(item_type) {}

AppListModelBuilder::~AppListModelBuilder() = default;

void AppListModelBuilder::Initialize(app_list::AppListSyncableService* service,
                                     Profile* profile,
                                     AppListModelUpdater* model_updater) {
  DCHECK(!service_ && !profile_ && !model_updater_);
  service_ = service;
  profile_ = profile;
  model_updater_ = model_updater;

  BuildModel();
}

void AppListModelBuilder::InsertApp(std::unique_ptr<ChromeAppListItem> app) {
  if (service_) {
    service_->AddItem(std::move(app));
    return;
  }

  // Initialize the position before adding `app`. In the product code, a new
  // app's position is initialized by `service_` if `app` does not have a
  // default position. But in tests `service_` could be null.
  DCHECK(position_setter_for_test_);
  position_setter_for_test_->Run(app.get());

  model_updater_->AddItem(std::move(app));
}

void AppListModelBuilder::RemoveApp(const std::string& id,
                                    bool unsynced_change) {
  // The parameter `is_uninstall` is true because the item is removed due to
  // local app uninstallation rather than sync.
  if (!unsynced_change && service_) {
    service_->RemoveItem(id, /*is_unistall=*/true);
    return;
  }

  model_updater_->RemoveItem(id, /*is_uninstall=*/true);
}

const app_list::AppListSyncableService::SyncItem*
AppListModelBuilder::GetSyncItem(
    const std::string& id,
    sync_pb::AppListSpecifics::AppListItemType type) {
  if (!service_)
    return nullptr;
  auto* result = service_->GetSyncItem(id);
  return result && result->item_type == type ? result : nullptr;
}

ChromeAppListItem* AppListModelBuilder::GetAppItem(const std::string& id) {
  ChromeAppListItem* item = model_updater_->FindItem(id);
  if (item && item->GetItemType() != item_type_) {
    VLOG(2) << "App Item matching id: " << id << " has incorrect type: '"
            << item->GetItemType() << "'";
    return nullptr;
  }
  return item;
}
