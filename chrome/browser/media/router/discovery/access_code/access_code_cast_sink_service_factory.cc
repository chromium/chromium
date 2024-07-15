// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service.h"
#include "components/media_router/browser/media_router_factory.h"

namespace media_router {

// static
AccessCodeCastSinkService* AccessCodeCastSinkServiceFactory::GetForProfile(
    Profile* profile) {
  DCHECK(profile);
  if (!GetAccessCodeCastEnabledPref(profile)) {
    return nullptr;
  }
  DCHECK(MediaRouterFactory::GetApiForBrowserContext(profile))
      << "The Media Router has not been properly intialized before the "
         "AccessCodeCastSinkService is created!";

  // GetServiceForBrowserContext returns a KeyedService hence the static_cast<>
  // to return a pointer to AccessCodeCastSinkService.
  AccessCodeCastSinkService* service = static_cast<AccessCodeCastSinkService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create */ false));
  if (!service) {
    // This can happen in certain cases (notably when a profile has been newly
    // created and a null ACCSS was associated with this profile before the
    // policies were downloaded. If we passed the above enabled check, then we
    // shouldn't have a null ACCSS pointer. So if we do, disassociate the null
    // and recreate (b/233285243).
    GetInstance()->Disassociate(profile);
    service = static_cast<AccessCodeCastSinkService*>(
        GetInstance()->GetServiceForBrowserContext(profile, /* create */ true));
  }
  DCHECK(service)
      << "No AccessCodeCastSinkService found for pref enabled user!";
  return service;
}

// static
AccessCodeCastSinkServiceFactory*
AccessCodeCastSinkServiceFactory::GetInstance() {
  static base::NoDestructor<AccessCodeCastSinkServiceFactory> instance;
  return instance.get();
}

AccessCodeCastSinkServiceFactory::AccessCodeCastSinkServiceFactory()
    : ProfileKeyedServiceFactory(
          "AccessCodeSinkService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  // TODO(b/238212430): Add a browsertest case to ensure that all media router
  // objects are created before the ACCSS.
  DependsOn(media_router::ChromeMediaRouterFactory::GetInstance());
}

AccessCodeCastSinkServiceFactory::~AccessCodeCastSinkServiceFactory() = default;

std::unique_ptr<KeyedService>
AccessCodeCastSinkServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);
  if (!profile || !GetAccessCodeCastEnabledPref(profile)) {
    return nullptr;
  }
  return std::make_unique<AccessCodeCastSinkService>(profile);
}

bool AccessCodeCastSinkServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace media_router
