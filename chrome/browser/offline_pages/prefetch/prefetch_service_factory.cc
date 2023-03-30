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
}  // namespace
PrefetchServiceFactory::PrefetchServiceFactory()
    : SimpleKeyedServiceFactory("OfflinePagePrefetchService",
                                SimpleDependencyManager::GetInstance()) {
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

  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});

  // TODO(crbug.com/1424920): Removing image fetcher references here breaks
  // tests: org.chromium.chrome.browser.ImageFetcherIntegrationTest Users of
  // image fetcher may be depending on this service to initialize the image
  // fetcher factory. [FATAL:scoped_refptr.h(291)] Check failed: ptr_.
  // ...
  // image_fetcher::GetImageFetcherService()
  GetImageFetcher(profile_key, image_fetcher::ImageFetcherConfig::kReducedMode);

  base::FilePath store_path =
      profile_key->GetPath().Append(chrome::kOfflinePagePrefetchStoreDirname);

  PrefetchStore::Delete(store_path, background_task_runner);
  return std::make_unique<PrefetchServiceImpl>();
}

}  // namespace offline_pages
