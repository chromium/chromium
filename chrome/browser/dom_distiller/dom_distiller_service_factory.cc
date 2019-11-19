// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"

#include <utility>

#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/dom_distiller/content/browser/distiller_page_web_contents.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/distiller.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/dom_distiller/distiller_ui_handle_android.h"
#endif  // defined(OS_ANDROID)

namespace dom_distiller {

DomDistillerContextKeyedService::DomDistillerContextKeyedService(
    std::unique_ptr<DistillerFactory> distiller_factory,
    std::unique_ptr<DistillerPageFactory> distiller_page_factory,
    std::unique_ptr<DistilledPagePrefs> distilled_page_prefs,
    std::unique_ptr<DistillerUIHandle> distiller_ui_handle)
    : DomDistillerService(std::move(distiller_factory),
                          std::move(distiller_page_factory),
                          std::move(distilled_page_prefs),
                          std::move(distiller_ui_handle)) {}

// static
DomDistillerServiceFactory* DomDistillerServiceFactory::GetInstance() {
  return base::Singleton<DomDistillerServiceFactory>::get();
}

// static
DomDistillerContextKeyedService*
DomDistillerServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DomDistillerContextKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

DomDistillerServiceFactory::DomDistillerServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "DomDistillerService",
          BrowserContextDependencyManager::GetInstance()) {
}

DomDistillerServiceFactory::~DomDistillerServiceFactory() {}

KeyedService* DomDistillerServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                       base::TaskPriority::BEST_EFFORT});

  base::FilePath database_dir(
      context->GetPath().Append(FILE_PATH_LITERAL("Articles")));

  std::unique_ptr<DistillerPageFactory> distiller_page_factory(
      new DistillerPageWebContentsFactory(context));
  std::unique_ptr<DistillerURLFetcherFactory> distiller_url_fetcher_factory(
      new DistillerURLFetcherFactory(
          content::BrowserContext::GetDefaultStoragePartition(context)
              ->GetURLLoaderFactoryForBrowserProcess()));

  dom_distiller::proto::DomDistillerOptions options;
  if (VLOG_IS_ON(1)) {
    options.set_debug_level(logging::GetVlogLevelHelper(
        FROM_HERE.file_name(), ::strlen(FROM_HERE.file_name())));
  }
  // Options for pagination algorithm:
  // - "next": detect anchors with "next" text
  // - "pagenum": detect anchors with numeric page numbers
  // Default is "next".
  options.set_pagination_algo("next");
  std::unique_ptr<DistillerFactory> distiller_factory(new DistillerFactoryImpl(
      std::move(distiller_url_fetcher_factory), options));
  std::unique_ptr<DistilledPagePrefs> distilled_page_prefs(
      new DistilledPagePrefs(profile->GetPrefs()));
  std::unique_ptr<DistillerUIHandle> distiller_ui_handle;

#if defined(OS_ANDROID)
  distiller_ui_handle =
      std::make_unique<dom_distiller::android::DistillerUIHandleAndroid>();
#endif  // defined(OS_ANDROID)

  DomDistillerContextKeyedService* service =
      new DomDistillerContextKeyedService(
          std::move(distiller_factory), std::move(distiller_page_factory),
          std::move(distilled_page_prefs), std::move(distiller_ui_handle));

  return service;
}

content::BrowserContext* DomDistillerServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Makes normal profile and off-the-record profile use same service instance.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace dom_distiller
