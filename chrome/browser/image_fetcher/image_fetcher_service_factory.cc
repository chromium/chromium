// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/common/chrome_paths_internal.h"
#include "components/image_fetcher/core/cache/image_cache.h"
#include "components/image_fetcher/core/cache/image_data_store_disk.h"
#include "components/image_fetcher/core/cache/image_metadata_store_leveldb.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/image_fetcher/image_fetcher_service_provider.h"
#endif

namespace {

// The path under the browser context's data directory which the image_cache
// will be stored.
const base::FilePath::CharType kImageCacheSubdir[] =
    FILE_PATH_LITERAL("image_cache");

base::FilePath GetCachePath(SimpleFactoryKey* key) {
  base::FilePath cache_path;
  chrome::GetUserCacheDirectory(key->GetPath(), &cache_path);
  return cache_path.Append(kImageCacheSubdir);
}

#if BUILDFLAG(IS_ANDROID)
image_fetcher::ImageFetcherService* GetImageFetcherService(
    SimpleFactoryKey* key) {
  return ImageFetcherServiceFactory::GetForKey(key);
}

std::string GetCachePathForJava(SimpleFactoryKey* key, std::string path) {
  base::FilePath cache_path;
  chrome::GetUserCacheDirectory(key->GetPath(), &cache_path);
  return cache_path.Append(kImageCacheSubdir).Append(path).MaybeAsASCII();
}
#endif

}  // namespace

// static
image_fetcher::ImageFetcherService* ImageFetcherServiceFactory::GetForKey(
    SimpleFactoryKey* key) {
  return static_cast<image_fetcher::ImageFetcherService*>(
      GetInstance()->GetServiceForKey(key, true));
}

// static
ImageFetcherServiceFactory* ImageFetcherServiceFactory::GetInstance() {
  static base::NoDestructor<ImageFetcherServiceFactory> instance;
  return instance.get();
}

ImageFetcherServiceFactory::ImageFetcherServiceFactory()
    : SimpleKeyedServiceFactory("ImageFetcherService",
                                SimpleDependencyManager::GetInstance()) {
// In order to move the android code to components, we need to push
// |GetImageFetcherService| to image_fetcher_bridge.
#if BUILDFLAG(IS_ANDROID)
  image_fetcher::SetImageFetcherServiceProvider(
      base::BindRepeating(&GetImageFetcherService));

  image_fetcher::SetImageFetcherCachePathProvider(
      base::BindRepeating(&GetCachePathForJava));
#endif
}

ImageFetcherServiceFactory::~ImageFetcherServiceFactory() = default;

std::unique_ptr<KeyedService>
ImageFetcherServiceFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  base::FilePath cache_path = GetCachePath(key);
  ProfileKey* profile_key = ProfileKey::FromSimpleFactoryKey(key);

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  base::DefaultClock* clock = base::DefaultClock::GetInstance();

  auto metadata_store =
      std::make_unique<image_fetcher::ImageMetadataStoreLevelDB>(
          profile_key->GetProtoDatabaseProvider(), cache_path, task_runner,
          clock);
  auto data_store = std::make_unique<image_fetcher::ImageDataStoreDisk>(
      cache_path, task_runner);

  scoped_refptr<image_fetcher::ImageCache> image_cache =
      base::MakeRefCounted<image_fetcher::ImageCache>(
          std::move(data_store), std::move(metadata_store),
          profile_key->GetPrefs(), clock, task_runner);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
  // Network is null for some tests, may be removable after
  // https://crbug.com/981057.
  if (SystemNetworkContextManager::GetInstance()) {
    url_loader_factory =
        SystemNetworkContextManager::GetInstance()->GetSharedURLLoaderFactory();
  }

  auto cached_image_fetcher_service =
      std::make_unique<image_fetcher::ImageFetcherService>(
          std::make_unique<ImageDecoderImpl>(), std::move(url_loader_factory),
          std::move(image_cache), key->IsOffTheRecord());
  return cached_image_fetcher_service;
}

SimpleFactoryKey* ImageFetcherServiceFactory::GetKeyToUse(
    SimpleFactoryKey* key) const {
  return key;
}
