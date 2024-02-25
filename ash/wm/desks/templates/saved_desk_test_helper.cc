// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_test_helper.h"

#include "ash/public/cpp/test/test_saved_desk_delegate.h"
#include "ash/shell.h"
#include "base/run_loop.h"
#include "components/desks_storage/core/admin_template_service.h"
#include "components/desks_storage/core/desk_test_util.h"
#include "components/desks_storage/core/local_desk_data_manager.h"
#include "components/prefs/testing_pref_service.h"

namespace ash {

SavedDeskTestHelper::SavedDeskTestHelper()
    : account_id_(AccountId::FromUserEmail("test@gmail.com")) {
  CHECK(desk_model_data_dir_.CreateUniqueTempDir());

  test_pref_service_ = std::make_unique<TestingPrefServiceSimple>();

  saved_desk_model_ = std::make_unique<desks_storage::LocalDeskDataManager>(
      desk_model_data_dir_.GetPath(), account_id_);

  // Creates the admin template service and its associated sub-directory within
  // the admin_template_service.  Allows us to test admin templates in desks
  // client.
  admin_template_service_ =
      std::make_unique<desks_storage::AdminTemplateService>(
          desk_model_data_dir_.GetPath(), account_id_,
          test_pref_service_.get());

  // Install desk model.
  static_cast<TestSavedDeskDelegate*>(Shell::Get()->saved_desk_delegate())
      ->set_desk_model(saved_desk_model_.get());

  // Install admin template service.
  static_cast<TestSavedDeskDelegate*>(Shell::Get()->saved_desk_delegate())
      ->set_admin_template_service(admin_template_service_.get());

  // Setup app registry cache.
  cache_ = std::make_unique<apps::AppRegistryCache>();
  desks_storage::desk_test_util::PopulateAppRegistryCache(account_id_,
                                                          cache_.get());

  // The admin template service requires that some app types be in the
  // initialized apps set, this method ensures that that set is populated
  // correctly.
  desks_storage::desk_test_util::PopulateAdminTestAppRegistryCache(
      account_id_, cache_.get());
}

SavedDeskTestHelper::~SavedDeskTestHelper() {
  static_cast<TestSavedDeskDelegate*>(Shell::Get()->saved_desk_delegate())
      ->set_desk_model(nullptr);
  cache_.reset();
  saved_desk_model_.reset();
  admin_template_service_.reset();

  // TODO(b/277753059): Temporary workaround that makes the timing issue less
  // likely to occur.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::DoNothingWithBoundArgs(std::move(desk_model_data_dir_)));
}

void SavedDeskTestHelper::AddAppIdToAppRegistryCache(
    const std::string& app_id) {
  desks_storage::desk_test_util::AddAppIdToAppRegistryCache(
      account_id_, cache_.get(), app_id.c_str());
}

void SavedDeskTestHelper::WaitForDeskModels() {
  while (
      !(saved_desk_model_->IsReady() && admin_template_service_->IsReady())) {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
}

}  // namespace ash
