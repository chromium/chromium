// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language/accept_languages_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/language/core/browser/accept_languages_service.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"

// static
AcceptLanguagesServiceFactory* AcceptLanguagesServiceFactory::GetInstance() {
  static base::NoDestructor<AcceptLanguagesServiceFactory> instance;
  return instance.get();
}

// static
language::AcceptLanguagesService*
AcceptLanguagesServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<language::AcceptLanguagesService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

AcceptLanguagesServiceFactory::AcceptLanguagesServiceFactory()
    : ProfileKeyedServiceFactory(
          "AcceptLanguagesService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

AcceptLanguagesServiceFactory::~AcceptLanguagesServiceFactory() = default;

std::unique_ptr<KeyedService>
AcceptLanguagesServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return std::make_unique<language::AcceptLanguagesService>(
      profile->GetPrefs(), language::prefs::kAcceptLanguages);
}
