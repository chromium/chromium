// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/ukm_database_client.h"

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_paths.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/segmentation_platform/internal/dummy_ukm_data_manager.h"
#include "components/segmentation_platform/internal/signals/ukm_observer.h"
#include "components/segmentation_platform/internal/ukm_data_manager_impl.h"
#include "components/segmentation_platform/public/features.h"
#include "components/ukm/ukm_service.h"

namespace segmentation_platform {

UkmDatabaseClient::UkmDatabaseClient() {
  if (base::FeatureList::IsEnabled(
          segmentation_platform::features::kSegmentationPlatformUkmEngine)) {
    ukm_data_manager_ = std::make_unique<UkmDataManagerImpl>();
  } else {
    ukm_data_manager_ = std::make_unique<DummyUkmDataManager>();
  }
}

UkmDatabaseClient::~UkmDatabaseClient() = default;

segmentation_platform::UkmDataManager* UkmDatabaseClient::GetUkmDataManager() {
  CHECK(ukm_data_manager_);
  return ukm_data_manager_.get();
}

void UkmDatabaseClient::PreProfileInit(bool in_memory_database) {
  if (ukm_recorder_for_testing_) {
    ukm_observer_ = std::make_unique<UkmObserver>(ukm_recorder_for_testing_);
  } else {
    ukm_observer_ = std::make_unique<UkmObserver>(
        g_browser_process->GetMetricsServicesManager()->GetUkmService());
  }

  // Path service is setup at early startup.
  base::FilePath local_data_dir;
  bool result = base::PathService::Get(chrome::DIR_USER_DATA, &local_data_dir);
  DCHECK(result);
  ukm_data_manager_->Initialize(
      local_data_dir.Append(FILE_PATH_LITERAL("segmentation_platform/ukm_db")),
      in_memory_database);
  ukm_data_manager_->StartObservation(ukm_observer_.get());
}

void UkmDatabaseClient::TearDownForTesting() {
  ukm_data_manager_.reset();
  ukm_observer_.reset();
  ukm_recorder_for_testing_ = nullptr;
}

void UkmDatabaseClient::PostMessageLoopRun() {
  // UkmService is destroyed in BrowserProcessImpl::TearDown(), which happens
  // after all the extra main parts get PostMainMessageLoopRun(). So, it is safe
  // to stop the observer here. The profiles can still be active and
  // UkmDataManager needs to be available. This does not tear down the
  // UkmDataManager, but only stops observing UKM.
  if (ukm_observer_) {
    // Some of the content browser implementations do not invoke
    // PreProfileInit().
    ukm_observer_->StopObserving();
  }
}

// static
UkmDatabaseClientHolder& UkmDatabaseClientHolder::GetInstance() {
  static base::NoDestructor<UkmDatabaseClientHolder> instance;
  return *instance;
}

// static
UkmDatabaseClient& UkmDatabaseClientHolder::GetClientInstance(
    Profile* profile) {
  UkmDatabaseClientHolder& instance = GetInstance();
  base::AutoLock l(instance.lock_);
  if (!instance.clients_for_testing_.empty()) {
    CHECK_IS_TEST();
    CHECK(profile);
    CHECK(instance.clients_for_testing_.count(profile));
    return *instance.clients_for_testing_[profile];
  }
  return *instance.main_client_;
}

// static
void UkmDatabaseClientHolder::SetUkmClientForTesting(
    Profile* profile,
    UkmDatabaseClient* client) {
  UkmDatabaseClientHolder& instance = GetInstance();
  instance.SetUkmClientForTestingInternal(profile, client);
}

UkmDatabaseClientHolder::UkmDatabaseClientHolder()
    : main_client_(std::make_unique<UkmDatabaseClient>()) {}

UkmDatabaseClientHolder::~UkmDatabaseClientHolder() = default;

void UkmDatabaseClientHolder::SetUkmClientForTestingInternal(
    Profile* profile,
    UkmDatabaseClient* client) {
  base::AutoLock l(lock_);
  CHECK(profile);
  if (client) {
    clients_for_testing_[profile] = client;
  } else {
    clients_for_testing_.erase(profile);
  }
}

}  // namespace segmentation_platform
