// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include <memory>

#include "base/files/file_path.h"
#include "base/hash/hash.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/client_util/local_tab_handler.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_config.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_profile_observer.h"
#include "chrome/browser/segmentation_platform/ukm_database_client.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/chrome_constants.h"
#include "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#include "components/segmentation_platform/embedder/input_delegate/tab_rank_dispatcher.h"
#include "components/segmentation_platform/embedder/input_delegate/tab_session_source.h"
#include "components/segmentation_platform/embedder/model_provider_factory_impl.h"
#include "components/segmentation_platform/embedder/tab_fetcher.h"
#include "components/segmentation_platform/internal/dummy_segmentation_platform_service.h"
#include "components/segmentation_platform/internal/segmentation_platform_service_impl.h"
#include "components/segmentation_platform/internal/ukm_data_manager.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_sessions/session_sync_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/segmentation_platform/client_util/tab_data_collection_util.h"
#endif

namespace segmentation_platform {
namespace {
const char kSegmentationPlatformProfileObserverKey[] =
    "segmentation_platform_profile_observer";
const char kSegmentationDeviceSwitcherUserDataKey[] =
    "segmentation_device_switcher_data";
const char kSegmentationTabRankDispatcherUserDataKey[] =
    "segmentation_tab_rank_dispatcher_data";

std::unique_ptr<processing::InputDelegateHolder> SetUpInputDelegates(
    std::vector<std::unique_ptr<Config>>& configs,
    sync_sessions::SessionSyncService* session_sync_service,
    TabFetcher* tab_fetcher) {
  auto input_delegate_holder =
      std::make_unique<processing::InputDelegateHolder>();
  for (auto& config : configs) {
    for (auto& id : config->input_delegates) {
      input_delegate_holder->SetDelegate(id.first, std::move(id.second));
    }
  }

  input_delegate_holder->SetDelegate(
      proto::CustomInput::FILL_TAB_METRICS,
      std::make_unique<segmentation_platform::processing::LocalTabSource>(
          session_sync_service, tab_fetcher));

  // Input delegates that are shared by multiple models.are added here.

  return input_delegate_holder;
}

void InitTabDataCollection(
    SegmentationPlatformService* service,
    sync_sessions::SessionSyncService* session_sync_service,
    std::unique_ptr<TabFetcher> tab_fetcher) {
  auto rank_dispatcher = std::make_unique<TabRankDispatcher>(
      service, session_sync_service, std::move(tab_fetcher));
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          features::kSegmentationPlatformCollectTabRankData)) {
    const char kSegmentationTabDataCollectionUtilUserDataKey[] =
        "segmentation_tab_tab_data_collection_util";
    auto tab_collection_util =
        std::make_unique<TabDataCollectionUtil>(service, rank_dispatcher.get());
    service->SetUserData(kSegmentationTabDataCollectionUtilUserDataKey,
                         std::move(tab_collection_util));
  }
#endif
  service->SetUserData(kSegmentationTabRankDispatcherUserDataKey,
                       std::move(rank_dispatcher));
}

}  // namespace

// static
SegmentationPlatformServiceFactory*
SegmentationPlatformServiceFactory::GetInstance() {
  static base::NoDestructor<SegmentationPlatformServiceFactory> instance;
  return instance.get();
}

// static
SegmentationPlatformService* SegmentationPlatformServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SegmentationPlatformService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

SegmentationPlatformServiceFactory::SegmentationPlatformServiceFactory()
    : ProfileKeyedServiceFactory(
          "SegmentationPlatformService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());
}

SegmentationPlatformServiceFactory::~SegmentationPlatformServiceFactory() =
    default;

KeyedService* SegmentationPlatformServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (context->IsOffTheRecord())
    return nullptr;

  if (!base::FeatureList::IsEnabled(features::kSegmentationPlatformFeature))
    return new DummySegmentationPlatformService();

  Profile* profile = Profile::FromBrowserContext(context);
  OptimizationGuideKeyedService* optimization_guide =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  sync_sessions::SessionSyncService* session_sync_service =
      SessionSyncServiceFactory::GetForProfile(profile);
  auto tab_fetcher = std::make_unique<processing::LocalTabHandler>(
      session_sync_service, profile);

  auto params = std::make_unique<SegmentationPlatformServiceImpl::InitParams>();
  auto profile_path = profile->GetPath().value();
  params->profile_id = base::NumberToString(base::PersistentHash(
      profile_path.data(),
      profile_path.length() * sizeof(base::FilePath::CharType)));
  params->history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
  params->task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  params->storage_dir =
      profile->GetPath().Append(chrome::kSegmentationPlatformStorageDirName);
  params->db_provider =
      profile->GetDefaultStoragePartition()->GetProtoDatabaseProvider();
  params->clock = base::DefaultClock::GetInstance();

  params->ukm_data_manager =
      UkmDatabaseClientHolder::GetClientInstance(profile).GetUkmDataManager();
  params->profile_prefs = profile->GetPrefs();
  params->configs = GetSegmentationPlatformConfig(context);
  params->input_delegate_holder = SetUpInputDelegates(
      params->configs, session_sync_service, tab_fetcher.get());
  params->field_trial_register = std::make_unique<FieldTrialRegisterImpl>();
  raw_ptr<FieldTrialRegister> field_trial_register =
      params->field_trial_register.get();
  params->model_provider = std::make_unique<ModelProviderFactoryImpl>(
      optimization_guide, params->configs, params->task_runner);
  // Guaranteed to outlive the SegmentationPlatformService, which depends on the
  // DeviceInfoSynceService.
  params->device_info_tracker =
      DeviceInfoSyncServiceFactory::GetForProfile(profile)
          ->GetDeviceInfoTracker();
  auto* service = new SegmentationPlatformServiceImpl(std::move(params));

  // Profile manager can be null in unit tests.
  if (g_browser_process->profile_manager()) {
    service->SetUserData(kSegmentationPlatformProfileObserverKey,
                         std::make_unique<SegmentationPlatformProfileObserver>(
                             service, g_browser_process->profile_manager()));
  }
  service->SetUserData(kSegmentationDeviceSwitcherUserDataKey,
                       std::make_unique<DeviceSwitcherResultDispatcher>(
                           service,
                           DeviceInfoSyncServiceFactory::GetForProfile(profile)
                               ->GetDeviceInfoTracker(),
                           profile->GetPrefs(), field_trial_register));

  InitTabDataCollection(service, session_sync_service, std::move(tab_fetcher));

  return service;
}

}  // namespace segmentation_platform
