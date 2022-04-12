// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/model_provider_factory_impl.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_config.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_profile_observer.h"
#include "chrome/browser/segmentation_platform/ukm_database_client.h"
#include "chrome/common/chrome_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/segmentation_platform/internal/dummy_segmentation_platform_service.h"
#include "components/segmentation_platform/internal/segmentation_platform_service_impl.h"
#include "components/segmentation_platform/internal/ukm_data_manager.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace segmentation_platform {
namespace {
const char kSegmentationPlatformProfileObserverKey[] =
    "segmentation_platform_profile_observer";

}  // namespace

// static
SegmentationPlatformServiceFactory*
SegmentationPlatformServiceFactory::GetInstance() {
  return base::Singleton<SegmentationPlatformServiceFactory>::get();
}

// static
SegmentationPlatformService* SegmentationPlatformServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SegmentationPlatformService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

SegmentationPlatformServiceFactory::SegmentationPlatformServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SegmentationPlatformService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
}

SegmentationPlatformServiceFactory::~SegmentationPlatformServiceFactory() =
    default;

KeyedService* SegmentationPlatformServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kSegmentationPlatformFeature))
    return new DummySegmentationPlatformService();

  Profile* profile = Profile::FromBrowserContext(context);
  OptimizationGuideKeyedService* optimization_guide =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::IMPLICIT_ACCESS);
  // If optimization guide feature is disabled, then disable segmentation.
  if (!optimization_guide)
    return new DummySegmentationPlatformService();

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  base::FilePath storage_dir =
      profile->GetPath().Append(chrome::kSegmentationPlatformStorageDirName);
  leveldb_proto::ProtoDatabaseProvider* db_provider =
      profile->GetDefaultStoragePartition()->GetProtoDatabaseProvider();
  base::DefaultClock* clock = base::DefaultClock::GetInstance();

  auto model_provider_factory = std::make_unique<ModelProviderFactoryImpl>(
      optimization_guide, task_runner);
  auto* ukm_data_manager = UkmDatabaseClient::GetInstance().GetUkmDataManager();
  ukm_data_manager->OnUkmAllowedStateChanged(
      g_browser_process->GetMetricsServicesManager()
          ->IsUkmAllowedForAllProfiles());

  auto* service = new SegmentationPlatformServiceImpl(
      std::move(model_provider_factory), db_provider, storage_dir,
      ukm_data_manager, profile->GetPrefs(), history_service, task_runner,
      clock, GetSegmentationPlatformConfig());

  service->SetUserData(kSegmentationPlatformProfileObserverKey,
                       std::make_unique<SegmentationPlatformProfileObserver>(
                           service, g_browser_process->profile_manager()));

  return service;
}

}  // namespace segmentation_platform
