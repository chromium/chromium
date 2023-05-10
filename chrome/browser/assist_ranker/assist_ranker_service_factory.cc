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
  return base::Singleton<AssistRankerServiceFactory>::get();
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
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

AssistRankerServiceFactory::~AssistRankerServiceFactory() {}

KeyedService* AssistRankerServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new AssistRankerServiceImpl(
      browser_context->GetPath(),
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory());
}

}  // namespace assist_ranker
