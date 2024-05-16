// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "start_suggest_service_factory.h"

#include <utility>

#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "components/search/start_suggest_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace search_resumption_module {
// static
StartSuggestService* StartSuggestServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<StartSuggestService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
StartSuggestServiceFactory* StartSuggestServiceFactory::GetInstance() {
  static base::NoDestructor<StartSuggestServiceFactory> instance;
  return instance.get();
}

StartSuggestServiceFactory::StartSuggestServiceFactory()
    : ProfileKeyedServiceFactory(
          "StartSuggestServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

StartSuggestServiceFactory::~StartSuggestServiceFactory() = default;

std::unique_ptr<KeyedService>
StartSuggestServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  auto url_loader_factory = context->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();
  return std::make_unique<StartSuggestService>(
      template_url_service, url_loader_factory,
      std::make_unique<ChromeAutocompleteSchemeClassifier>(profile),
      std::string(), std::string(), GURL(chrome::kChromeUINewTabURL));
}

}  // namespace search_resumption_module
