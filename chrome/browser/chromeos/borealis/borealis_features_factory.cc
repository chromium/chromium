// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_features_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace borealis {
BorealisFeatures* BorealisFeaturesFactory::GetForProfile(Profile* profile) {
  return static_cast<BorealisFeatures*>(
      GetInstance()->GetServiceForBrowserContext(profile,
                                                 /* create */ true));
}

BorealisFeaturesFactory* BorealisFeaturesFactory::GetInstance() {
  static base::NoDestructor<BorealisFeaturesFactory> factory;
  return factory.get();
}

// This service does not depend on any other services.
BorealisFeaturesFactory::BorealisFeaturesFactory()
    : BrowserContextKeyedServiceFactory(
          "BorealisFeaturesService",
          BrowserContextDependencyManager::GetInstance()) {}

BorealisFeaturesFactory::~BorealisFeaturesFactory() = default;

KeyedService* BorealisFeaturesFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new BorealisFeatures(Profile::FromBrowserContext(context));
}

}  // namespace borealis
