// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_test_helper.h"

#include "ash/public/cpp/test/test_desks_templates_delegate.h"
#include "ash/shell.h"
#include "base/run_loop.h"
#include "components/desks_storage/core/desk_test_util.h"
#include "components/desks_storage/core/local_desk_data_manager.h"

namespace ash {

SavedDeskTestHelper::SavedDeskTestHelper()
    : account_id_(AccountId::FromUserEmail("test@gmail.com")) {
  CHECK(desk_model_data_dir_.CreateUniqueTempDir());

  desk_model_ = std::make_unique<desks_storage::LocalDeskDataManager>(
      desk_model_data_dir_.GetPath(), account_id_);

  desks_storage::LocalDeskDataManager::
      SetExcludeSaveAndRecallDeskInMaxEntryCountForTesting(false);

  // Install desk model.
  static_cast<TestDesksTemplatesDelegate*>(
      Shell::Get()->desks_templates_delegate())
      ->set_desk_model(desk_model_.get());

  // Setup app registry cache.
  cache_ = std::make_unique<apps::AppRegistryCache>();
  desks_storage::desk_test_util::PopulateAppRegistryCache(account_id_,
                                                          cache_.get());
}

SavedDeskTestHelper::~SavedDeskTestHelper() {
  static_cast<TestDesksTemplatesDelegate*>(
      Shell::Get()->desks_templates_delegate())
      ->set_desk_model(nullptr);
}

void SavedDeskTestHelper::AddAppIdToAppRegistryCache(
    const std::string& app_id) {
  desks_storage::desk_test_util::AddAppIdToAppRegistryCache(
      account_id_, cache_.get(), app_id.c_str());
}

void SavedDeskTestHelper::WaitForDeskModel() {
  while (!desk_model_->IsReady()) {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
}

}  // namespace ash
