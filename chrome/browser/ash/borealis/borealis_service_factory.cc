// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_service_factory.h"

#include "chrome/browser/ash/borealis/borealis_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace borealis {
BorealisService* BorealisServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<BorealisService*>(
      GetInstance()->GetServiceForBrowserContext(profile,
                                                 /* create */ true));
}

BorealisServiceFactory* BorealisServiceFactory::GetInstance() {
  static base::NoDestructor<BorealisServiceFactory> factory;
  return factory.get();
}

// This service does not depend on any other services.
BorealisServiceFactory::BorealisServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "BorealisService",
          BrowserContextDependencyManager::GetInstance()) {}

BorealisServiceFactory::~BorealisServiceFactory() = default;

KeyedService* BorealisServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new BorealisServiceImpl(Profile::FromBrowserContext(context));
}

bool BorealisServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace borealis
