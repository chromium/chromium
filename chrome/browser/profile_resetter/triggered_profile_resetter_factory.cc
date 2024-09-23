// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/triggered_profile_resetter_factory.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/profile_resetter/triggered_profile_resetter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_context.h"

// static
TriggeredProfileResetter* TriggeredProfileResetterFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<TriggeredProfileResetter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
TriggeredProfileResetterFactory*
TriggeredProfileResetterFactory::GetInstance() {
  static base::NoDestructor<TriggeredProfileResetterFactory> instance;
  return instance.get();
}

TriggeredProfileResetterFactory::TriggeredProfileResetterFactory()
    : ProfileKeyedServiceFactory(
          "TriggeredProfileResetter",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

TriggeredProfileResetterFactory::~TriggeredProfileResetterFactory() = default;

std::unique_ptr<KeyedService>
TriggeredProfileResetterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  auto service = std::make_unique<TriggeredProfileResetter>(profile);
  service->Activate();
  return service;
}

void TriggeredProfileResetterFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if BUILDFLAG(IS_WIN)
  registry->RegisterInt64Pref(prefs::kLastProfileResetTimestamp, 0L);
#endif
}

bool TriggeredProfileResetterFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}
