// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace media_router {

// static
AccessCodeCastSinkService* AccessCodeCastSinkServiceFactory::GetForProfile(
    Profile* profile) {
  DCHECK(profile);
  if (!GetAccessCodeCastEnabledPref(profile->GetPrefs())) {
    return nullptr;
  }
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
  return base::Singleton<AccessCodeCastSinkServiceFactory>::get();
}

AccessCodeCastSinkServiceFactory::AccessCodeCastSinkServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "AccessCodeSinkService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(media_router::ChromeMediaRouterFactory::GetInstance());
}

AccessCodeCastSinkServiceFactory::~AccessCodeCastSinkServiceFactory() = default;

KeyedService* AccessCodeCastSinkServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  if (!GetAccessCodeCastEnabledPref(
          Profile::FromBrowserContext(profile)->GetPrefs())) {
    return nullptr;
  }
  return new AccessCodeCastSinkService(static_cast<Profile*>(profile));
}

bool AccessCodeCastSinkServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool AccessCodeCastSinkServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace media_router
