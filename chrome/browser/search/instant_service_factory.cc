// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service_factory.h"

#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "components/search/search.h"

namespace {

BASE_FEATURE(kProfileBasedInstantService,
             "ProfileBasedInstantService",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

// static
InstantService* InstantServiceFactory::GetForProfile(Profile* profile) {
  DCHECK(search::IsInstantExtendedAPIEnabled());
  TRACE_EVENT0("loading", "InstantServiceFactory::GetForProfile");
  if (base::FeatureList::IsEnabled(kProfileBasedInstantService)) {
    if (!profile->instant_service()) {
      profile->set_instant_service(static_cast<InstantService*>(
          GetInstance()->GetServiceForBrowserContext(profile, true)));
    }
    return profile->instant_service().value();
  }

  return static_cast<InstantService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
InstantServiceFactory* InstantServiceFactory::GetInstance() {
  static base::NoDestructor<InstantServiceFactory> instance;
  return instance.get();
}

InstantServiceFactory::InstantServiceFactory()
    : ProfileKeyedServiceFactory(
          "InstantService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(ThemeServiceFactory::GetInstance());
}

InstantServiceFactory::~InstantServiceFactory() = default;

std::unique_ptr<KeyedService>
InstantServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  DCHECK(search::IsInstantExtendedAPIEnabled());
  return std::make_unique<InstantService>(Profile::FromBrowserContext(context));
}

void InstantServiceFactory::BrowserContextDestroyed(
    content::BrowserContext* browser_context) {
  Profile::FromBrowserContext(browser_context)->set_instant_service(nullptr);
  BrowserContextKeyedServiceFactory::BrowserContextDestroyed(browser_context);
}
