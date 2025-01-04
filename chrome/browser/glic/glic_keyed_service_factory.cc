// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_keyed_service_factory.h"

#include "chrome/browser/glic/glic_profile_manager.h"

namespace glic {

// static
GlicKeyedService* GlicKeyedServiceFactory::GetGlicKeyedService(
    content::BrowserContext* browser_context) {
  return static_cast<GlicKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
GlicKeyedServiceFactory* GlicKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<GlicKeyedServiceFactory> factory;
  return factory.get();
}

GlicKeyedServiceFactory::GlicKeyedServiceFactory()
    : ProfileKeyedServiceFactory("GlicKeyedService",
                                 ProfileSelections::BuildForRegularProfile()) {}

GlicKeyedServiceFactory::~GlicKeyedServiceFactory() = default;

bool GlicKeyedServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return false;
}

std::unique_ptr<KeyedService>
GlicKeyedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<GlicKeyedService>(context,
                                            GlicProfileManager::GetInstance());
}

}  // namespace glic
