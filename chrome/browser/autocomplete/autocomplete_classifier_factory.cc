// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"

#include <memory>

#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/autocomplete/in_memory_url_index_factory.h"
#include "chrome/browser/autocomplete/remote_suggestions_service_factory.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#endif

// static
AutocompleteClassifier* AutocompleteClassifierFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AutocompleteClassifier*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AutocompleteClassifierFactory* AutocompleteClassifierFactory::GetInstance() {
  static base::NoDestructor<AutocompleteClassifierFactory> instance;
  return instance.get();
}

// static
std::unique_ptr<KeyedService> AutocompleteClassifierFactory::BuildInstanceFor(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  return std::make_unique<AutocompleteClassifier>(
      std::make_unique<AutocompleteController>(
          std::make_unique<ChromeAutocompleteProviderClient>(profile),
          AutocompleteClassifier::DefaultOmniboxProviders()),
      std::make_unique<ChromeAutocompleteSchemeClassifier>(profile));
}

AutocompleteClassifierFactory::AutocompleteClassifierFactory()
    : ProfileKeyedServiceFactory(
          "AutocompleteClassifier",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
#endif
  DependsOn(TemplateURLServiceFactory::GetInstance());
  DependsOn(ShortcutsBackendFactory::GetInstance());
  DependsOn(InMemoryURLIndexFactory::GetInstance());
  DependsOn(RemoteSuggestionsServiceFactory::GetInstance());
}

AutocompleteClassifierFactory::~AutocompleteClassifierFactory() = default;

bool AutocompleteClassifierFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

std::unique_ptr<KeyedService>
AutocompleteClassifierFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* profile) const {
  return BuildInstanceFor(static_cast<Profile*>(profile));
}
