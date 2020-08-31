// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/kaleidoscope/kaleidoscope_service_factory.h"

#include "chrome/browser/media/kaleidoscope/kaleidoscope_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace kaleidoscope {

// static
KaleidoscopeService* KaleidoscopeServiceFactory::GetForProfile(
    Profile* profile) {
  if (profile->IsOffTheRecord())
    return nullptr;

  if (!KaleidoscopeService::IsEnabled())
    return nullptr;

  return static_cast<KaleidoscopeService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
KaleidoscopeServiceFactory* KaleidoscopeServiceFactory::GetInstance() {
  static base::NoDestructor<KaleidoscopeServiceFactory> factory;
  return factory.get();
}

KaleidoscopeServiceFactory::KaleidoscopeServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "KaleidoscopeService",
          BrowserContextDependencyManager::GetInstance()) {}

KaleidoscopeServiceFactory::~KaleidoscopeServiceFactory() = default;

bool KaleidoscopeServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

KeyedService* KaleidoscopeServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(!context->IsOffTheRecord());

  if (!KaleidoscopeService::IsEnabled())
    return nullptr;

  return new KaleidoscopeService(Profile::FromBrowserContext(context));
}

}  // namespace kaleidoscope
