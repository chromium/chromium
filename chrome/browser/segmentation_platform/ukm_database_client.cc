// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/ukm_database_client.h"

#include <utility>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_paths.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/segmentation_platform/internal/dummy_ukm_data_manager.h"
#include "components/segmentation_platform/internal/ukm_data_manager_impl.h"
#include "components/segmentation_platform/public/features.h"
#include "components/ukm/ukm_service.h"

namespace segmentation_platform {

// static
UkmDatabaseClient& UkmDatabaseClient::GetInstance() {
  static base::NoDestructor<UkmDatabaseClient> instance;
  return *instance;
}

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

void UkmDatabaseClient::PreProfileInit() {
  // Path service is setup at early startup.
  base::FilePath local_data_dir;
  bool result = base::PathService::Get(chrome::DIR_USER_DATA, &local_data_dir);
  DCHECK(result);
  ukm_data_manager_->Initialize(
      local_data_dir.Append(FILE_PATH_LITERAL("segmentation_platform/ukm_db")));

  // Metrics service is created at PreCreateThreads, and should be available at
  // PreProfileInit(). If this changes the metrics service is created on-demand,
  // so will get created by calling GetMetricsServicesManager().
  ukm_data_manager_->NotifyCanObserveUkm(
      g_browser_process->GetMetricsServicesManager()->GetUkmService());
}

void UkmDatabaseClient::PostMessageLoopRun() {
  // UkmService is destroyed in BrowserProcessImpl::TearDown(), which happens
  // after all the extra main parts get PostMainMessageLoopRun(). So, it is safe
  // to remove observer here. The profiles can still be active and
  // UkmDataManager needs to be available. This does not tear down the
  // UkmDataManager, but only stops observing UKM.
  ukm_data_manager_->StopObservingUkm();
}

void UkmDatabaseClient::ProfileManagerDestroying() {
  // All profiles are destroyed, it it safe to tear down the UkmDataManager.
  ukm_data_manager_.reset();
}

}  // namespace segmentation_platform
