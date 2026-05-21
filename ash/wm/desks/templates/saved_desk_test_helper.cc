// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_test_helper.h"

#include "ash/public/cpp/test/test_saved_desk_delegate.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_multi_source_observation.h"
#include "components/desks_storage/core/admin_template_service.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/desks_storage/core/desk_model_observer.h"
#include "components/desks_storage/core/desk_test_util.h"
#include "components/desks_storage/core/local_desk_data_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"

namespace ash {
namespace {

class DeskModelsReadyWaiter : public desks_storage::DeskModelObserver {
 public:
  DeskModelsReadyWaiter(
      desks_storage::DeskModel* saved_desk_model,
      desks_storage::AdminTemplateService* admin_template_service)
      : saved_desk_model_(saved_desk_model),
        admin_template_service_(admin_template_service) {
    observations_.AddObservation(saved_desk_model_);
    observations_.AddObservation(admin_template_service_->GetFullDeskModel());
  }

  DeskModelsReadyWaiter(const DeskModelsReadyWaiter&) = delete;
  DeskModelsReadyWaiter& operator=(const DeskModelsReadyWaiter&) = delete;

  ~DeskModelsReadyWaiter() override = default;

  void Wait() {
    if (AreDeskModelsReady()) {
      return;
    }
    run_loop_.Run();
  }

  // desks_storage::DeskModelObserver:
  void DeskModelLoaded() override {
    if (AreDeskModelsReady()) {
      run_loop_.Quit();
    }
  }

 private:
  bool AreDeskModelsReady() const {
    return saved_desk_model_->IsReady() && admin_template_service_->IsReady();
  }

  raw_ptr<desks_storage::DeskModel> saved_desk_model_;
  raw_ptr<desks_storage::AdminTemplateService> admin_template_service_;
  base::RunLoop run_loop_;
  base::ScopedMultiSourceObservation<desks_storage::DeskModel,
                                     desks_storage::DeskModelObserver>
      observations_{this};
};

}  // namespace

SavedDeskTestHelper::SavedDeskTestHelper() {
  CHECK(desk_model_data_dir_.CreateUniqueTempDir());
  scoped_observation_.Observe(Shell::Get()->session_controller());
}

SavedDeskTestHelper::~SavedDeskTestHelper() {
  cache_map_.clear();
  // TODO(b/277753059): Temporary workaround that makes the timing issue less
  // likely to occur.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::DoNothingWithBoundArgs(std::move(desk_model_data_dir_)));
}

void SavedDeskTestHelper::OnUserSessionAdded(const AccountId& account_id) {
  auto cache = std::make_unique<apps::AppRegistryCache>();
  auto* cache_ptr = cache.get();
  if (cache_map_.count(account_id)) {
    // TODO(crbug.com/383442863): Remove this once the misused of
    // SimulateUserLogin has been fixed.
    return;
  }

  cache_map_.emplace(account_id, std::move(cache));
  apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(account_id,
                                                           cache_ptr);

  // Setup app registry cache.
  desks_storage::desk_test_util::PopulateAppRegistryCache(account_id,
                                                          cache_ptr);

  // The admin template service requires that some app types be in the
  // initialized apps set, this method ensures that that set is populated
  // correctly.
  desks_storage::desk_test_util::PopulateAdminTestAppRegistryCache(account_id,
                                                                   cache_ptr);
}

void SavedDeskTestHelper::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  auto* cache =
      apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id);
  CHECK(cache);
  account_id_ = account_id;

  // Install desk model.
  auto* saved_desk_delegate =
      static_cast<TestSavedDeskDelegate*>(Shell::Get()->saved_desk_delegate());
  saved_desk_delegate->set_desk_model(nullptr);
  saved_desk_delegate->set_admin_template_service(nullptr);

  saved_desk_model_ = std::make_unique<desks_storage::LocalDeskDataManager>(
      desk_model_data_dir_.GetPath(), account_id_);

  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  CHECK(pref_service);

  // Creates the admin template service and its associated sub-directory within
  // the admin_template_service.  Allows us to test admin templates in desks
  // client.
  admin_template_service_ =
      std::make_unique<desks_storage::AdminTemplateService>(
          desk_model_data_dir_.GetPath(), account_id_, pref_service);

  // Install desk model.
  saved_desk_delegate->set_desk_model(saved_desk_model_.get());

  // Install admin template service.
  saved_desk_delegate->set_admin_template_service(
      admin_template_service_.get());
}

void SavedDeskTestHelper::Shutdown() {
  scoped_observation_.Reset();
  auto* saved_desk_delegate =
      static_cast<TestSavedDeskDelegate*>(Shell::Get()->saved_desk_delegate());
  saved_desk_delegate->set_desk_model(nullptr);
  saved_desk_delegate->set_admin_template_service(nullptr);
  saved_desk_model_.reset();
  admin_template_service_.reset();
}

void SavedDeskTestHelper::AddAppIdToAppRegistryCache(
    const std::string& app_id) {
  auto* cache =
      apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id_);
  desks_storage::desk_test_util::AddAppIdToAppRegistryCache(account_id_, cache,
                                                            app_id.c_str());
}

void SavedDeskTestHelper::WaitForDeskModels() {
  DeskModelsReadyWaiter(saved_desk_model_.get(), admin_template_service_.get())
      .Wait();
}

}  // namespace ash
