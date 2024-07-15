// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/assist_ranker/assist_ranker_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "components/assist_ranker/assist_ranker_service_impl.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace assist_ranker {

// static
AssistRankerServiceFactory* AssistRankerServiceFactory::GetInstance() {
  static base::NoDestructor<AssistRankerServiceFactory> instance;
  return instance.get();
}

// static
AssistRankerService* AssistRankerServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<AssistRankerService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

AssistRankerServiceFactory::AssistRankerServiceFactory()
    : ProfileKeyedServiceFactory(
          "AssistRankerService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

AssistRankerServiceFactory::~AssistRankerServiceFactory() = default;

std::unique_ptr<KeyedService>
AssistRankerServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  return std::make_unique<AssistRankerServiceImpl>(
      browser_context->GetPath(),
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory());
}

}  // namespace assist_ranker
