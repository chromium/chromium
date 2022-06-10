// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_service_factory.h"

#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace bruschetta {

// static
BruschettaService* bruschetta::BruschettaServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<BruschettaService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
BruschettaServiceFactory* BruschettaServiceFactory::GetInstance() {
  static base::NoDestructor<BruschettaServiceFactory> factory;
  return factory.get();
}

BruschettaServiceFactory::BruschettaServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "BruschettaService",
          BrowserContextDependencyManager::GetInstance()) {}

BruschettaServiceFactory::~BruschettaServiceFactory() = default;

KeyedService* BruschettaServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new BruschettaService(Profile::FromBrowserContext(context));
}

}  // namespace bruschetta
