// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_mime_types_service_factory.h"

#include "chrome/browser/ash/crostini/crostini_mime_types_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace crostini {

// static
CrostiniMimeTypesService* CrostiniMimeTypesServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<CrostiniMimeTypesService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CrostiniMimeTypesServiceFactory*
CrostiniMimeTypesServiceFactory::GetInstance() {
  static base::NoDestructor<CrostiniMimeTypesServiceFactory> factory;
  return factory.get();
}

CrostiniMimeTypesServiceFactory::CrostiniMimeTypesServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "CrostiniMimeTypesService",
          BrowserContextDependencyManager::GetInstance()) {}

CrostiniMimeTypesServiceFactory::~CrostiniMimeTypesServiceFactory() = default;

KeyedService* CrostiniMimeTypesServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new CrostiniMimeTypesService(profile);
}

}  // namespace crostini
