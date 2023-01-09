// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/singleton.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/download/background_download_service_factory.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/prefetch/gcm_token.h"
#include "chrome/browser/offline_pages/prefetch/offline_metrics_collector_impl.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_background_task_handler_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/transition_manager/full_browser_transition_manager.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/feed/feed_feature_list.h"
#include "components/image_fetcher/core/cached_image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher_impl.h"
#include "components/offline_pages/core/prefetch/prefetch_downloader_impl.h"
#include "components/offline_pages/core/prefetch/prefetch_gcm_app_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_importer_impl.h"
#include "components/offline_pages/core/prefetch/prefetch_network_request_factory_impl.h"
#include "components/offline_pages/core/prefetch/prefetch_service_impl.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace offline_pages {

namespace {

image_fetcher::ImageFetcher* GetImageFetcher(
    ProfileKey* key,
    image_fetcher::ImageFetcherConfig config) {
  image_fetcher::ImageFetcherService* image_fetcher_service =
      ImageFetcherServiceFactory::GetForKey(key);
  DCHECK(image_fetcher_service);
  return image_fetcher_service->GetImageFetcher(config);
}

void SwitchToFullBrowserImageFetcher(PrefetchServiceImpl* prefetch_service,
                                     ProfileKey* key) {
  // We don't need to switch the image_fetcher if it isn't created.
  if (!prefetch_service->GetImageFetcher())
    return;

  prefetch_service->ReplaceImageFetcher(
      GetImageFetcher(key, image_fetcher::ImageFetcherConfig::kDiskCacheOnly));
}

void OnProfileCreated(PrefetchServiceImpl* prefetch_service, Profile* profile) {
  if (IsPrefetchingOfflinePagesEnabled()) {
    // Trigger an update of the cached GCM token. This needs to be post tasked
    // because otherwise leads to circular dependency between
    // PrefetchServiceFactory and GCMProfileServiceFactory. See
    // https://crbug.com/944952
    // Update is not a priority so make sure it happens after the critical
    // startup path.
    content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
        ->PostTask(FROM_HERE,
                   base::BindOnce(
                       &GetGCMToken, profile, kPrefetchingOfflinePagesAppId,
                       base::BindOnce(&PrefetchServiceImpl::GCMTokenReceived,
                                      prefetch_service->GetWeakPtr())));
  }

  SwitchToFullBrowserImageFetcher(prefetch_service, profile->GetProfileKey());
}

}  // namespace

PrefetchServiceFactory::PrefetchServiceFactory()
    : SimpleKeyedServiceFactory("OfflinePagePrefetchService",
                                SimpleDependencyManager::GetInstance()) {
  DependsOn(BackgroundDownloadServiceFactory::GetInstance());
  DependsOn(OfflinePageModelFactory::GetInstance());
  DependsOn(ImageFetcherServiceFactory::GetInstance());
}

// static
PrefetchServiceFactory* PrefetchServiceFactory::GetInstance() {
  return base::Singleton<PrefetchServiceFactory>::get();
}

// static
PrefetchService* PrefetchServiceFactory::GetForKey(SimpleFactoryKey* key) {
  return static_cast<PrefetchService*>(
      GetInstance()->GetServiceForKey(key, true));
}

std::unique_ptr<KeyedService> PrefetchServiceFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  ProfileKey* profile_key = ProfileKey::FromSimpleFactoryKey(key);

  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForKey(profile_key);
  DCHECK(offline_page_model);

  auto offline_metrics_collector =
      std::make_unique<OfflineMetricsCollectorImpl>(profile_key->GetPrefs());

  auto prefetch_dispatcher =
      std::make_unique<PrefetchDispatcherImpl>(profile_key->GetPrefs());

  auto* system_network_context_manager =
      SystemNetworkContextManager::GetInstance();
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
  if (system_network_context_manager) {
    url_loader_factory =
        system_network_context_manager->GetSharedURLLoaderFactory();
  } else {
    // In unit_tests, NetworkService might not be available and
    // |system_network_context_manager| would be null.
    url_loader_factory = nullptr;
  }

  auto prefetch_network_request_factory =
      std::make_unique<PrefetchNetworkRequestFactoryImpl>(
          url_loader_factory, chrome::GetChannel(),
          embedder_support::GetUserAgent(), profile_key->GetPrefs());

  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  base::FilePath store_path =
      profile_key->GetPath().Append(chrome::kOfflinePagePrefetchStoreDirname);
  auto prefetch_store =
      std::make_unique<PrefetchStore>(background_task_runner, store_path);

  image_fetcher::ImageFetcher* image_fetcher = GetImageFetcher(
      profile_key, image_fetcher::ImageFetcherConfig::kReducedMode);

  auto prefetch_downloader = std::make_unique<PrefetchDownloaderImpl>(
      BackgroundDownloadServiceFactory::GetForKey(profile_key),
      chrome::GetChannel(), profile_key->GetPrefs());

  auto prefetch_importer = std::make_unique<PrefetchImporterImpl>(
      prefetch_dispatcher.get(), offline_page_model, background_task_runner);

  auto prefetch_background_task_handler =
      std::make_unique<PrefetchBackgroundTaskHandlerImpl>(
          profile_key->GetPrefs());

  auto service = std::make_unique<PrefetchServiceImpl>(
      std::move(offline_metrics_collector), std::move(prefetch_dispatcher),
      std::move(prefetch_network_request_factory), offline_page_model,
      std::move(prefetch_store), std::move(prefetch_downloader),
      std::move(prefetch_importer), std::make_unique<PrefetchGCMAppHandler>(),
      std::move(prefetch_background_task_handler), image_fetcher,
      profile_key->GetPrefs());

  auto callback = base::BindOnce(&OnProfileCreated, service.get());
  FullBrowserTransitionManager::Get()->RegisterCallbackOnProfileCreation(
      profile_key, std::move(callback));

  return service;
}

}  // namespace offline_pages
