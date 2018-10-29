// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_service_factory.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/background_fetch/background_fetch_download_client.h"
#include "chrome/browser/download/download_task_scheduler_impl.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "components/download/content/factory/download_service_factory.h"
#include "components/download/public/background_service/clients.h"
#include "components/download/public/background_service/download_service.h"
#include "components/download/public/background_service/features.h"
#include "components/download/public/background_service/task_scheduler.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/download/service/download_task_scheduler.h"
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/prefetch/offline_prefetch_download_client.h"
#endif

// static
DownloadServiceFactory* DownloadServiceFactory::GetInstance() {
  return base::Singleton<DownloadServiceFactory>::get();
}

// static
download::DownloadService* DownloadServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<download::DownloadService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

DownloadServiceFactory::DownloadServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "download::DownloadService",
          BrowserContextDependencyManager::GetInstance()) {}

DownloadServiceFactory::~DownloadServiceFactory() = default;

KeyedService* DownloadServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto clients = std::make_unique<download::DownloadClientMap>();

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  // Offline prefetch doesn't support incognito.
  if (!context->IsOffTheRecord()) {
    clients->insert(std::make_pair(
        download::DownloadClient::OFFLINE_PAGE_PREFETCH,
        std::make_unique<offline_pages::OfflinePrefetchDownloadClient>(
            context)));
  }
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

  clients->insert(
      std::make_pair(download::DownloadClient::BACKGROUND_FETCH,
                     std::make_unique<BackgroundFetchDownloadClient>(context)));

  // Build in memory download service for incognito profile.
  if (context->IsOffTheRecord() &&
      base::FeatureList::IsEnabled(download::kDownloadServiceIncognito)) {
    content::BrowserContext::BlobContextGetter blob_context_getter =
        content::BrowserContext::GetBlobStorageContext(context);
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
        base::CreateSingleThreadTaskRunnerWithTraits(
            {content::BrowserThread::IO});

    return download::BuildInMemoryDownloadService(
        context, std::move(clients), content::GetNetworkConnectionTracker(),
        base::FilePath(), blob_context_getter, io_task_runner);
  } else {
    // Build download service for normal profile.
    base::FilePath storage_dir;
    if (!context->IsOffTheRecord() && !context->GetPath().empty()) {
      storage_dir =
          context->GetPath().Append(chrome::kDownloadServiceStorageDirname);
    }
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

    std::unique_ptr<download::TaskScheduler> task_scheduler;
#if defined(OS_ANDROID)
    task_scheduler =
        std::make_unique<download::android::DownloadTaskScheduler>();
#else
    task_scheduler = std::make_unique<DownloadTaskSchedulerImpl>(context);
#endif

    return download::BuildDownloadService(
        context, std::move(clients), content::GetNetworkConnectionTracker(),
        storage_dir, background_task_runner, std::move(task_scheduler));
  }
}

content::BrowserContext* DownloadServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
