// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"

#include <memory>

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "components/omnibox/browser/shortcuts_constants.h"
#include "components/prefs/pref_service.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/autocomplete/shortcuts_extensions_manager.h"

namespace {
const char kShortcutsExtensionsManagerKey[] = "ShortcutsExtensionsManager";
}
#endif

// static
scoped_refptr<ShortcutsBackend> ShortcutsBackendFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ShortcutsBackend*>(
      GetInstance()->GetServiceForBrowserContext(profile, true).get());
}

// static
scoped_refptr<ShortcutsBackend> ShortcutsBackendFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<ShortcutsBackend*>(
      GetInstance()->GetServiceForBrowserContext(profile, false).get());
}

// static
ShortcutsBackendFactory* ShortcutsBackendFactory::GetInstance() {
  static base::NoDestructor<ShortcutsBackendFactory> instance;
  return instance.get();
}

// static
scoped_refptr<RefcountedKeyedService>
ShortcutsBackendFactory::BuildProfileForTesting(
    content::BrowserContext* profile) {
  return CreateShortcutsBackend(Profile::FromBrowserContext(profile), false);
}

// static
scoped_refptr<RefcountedKeyedService>
ShortcutsBackendFactory::BuildProfileNoDatabaseForTesting(
    content::BrowserContext* profile) {
  return CreateShortcutsBackend(Profile::FromBrowserContext(profile), true);
}

ShortcutsBackendFactory::ShortcutsBackendFactory()
    : RefcountedProfileKeyedServiceFactory(
          "ShortcutsBackend",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

ShortcutsBackendFactory::~ShortcutsBackendFactory() = default;

scoped_refptr<RefcountedKeyedService>
ShortcutsBackendFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return CreateShortcutsBackend(Profile::FromBrowserContext(profile), false);
}

bool ShortcutsBackendFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

void ShortcutsBackendFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  context->RemoveUserData(kShortcutsExtensionsManagerKey);
#endif

  RefcountedBrowserContextKeyedServiceFactory::BrowserContextShutdown(context);
}

// static
scoped_refptr<ShortcutsBackend> ShortcutsBackendFactory::CreateShortcutsBackend(
    Profile* profile,
    bool suppress_db) {
  scoped_refptr<ShortcutsBackend> backend(new ShortcutsBackend(
      TemplateURLServiceFactory::GetForProfile(profile),
      std::make_unique<UIThreadSearchTermsData>(),
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      profile->GetPath().Append(kShortcutsDatabaseName), suppress_db));
#if BUILDFLAG(ENABLE_EXTENSIONS)
  auto extensions_manager =
      std::make_unique<ShortcutsExtensionsManager>(profile);
  profile->SetUserData(kShortcutsExtensionsManagerKey,
                       std::move(extensions_manager));
#endif
  return backend->Init() ? backend : nullptr;
}
