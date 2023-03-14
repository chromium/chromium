// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/borealis/borealis_service_impl.h"
#include "chrome/browser/profiles/profile.h"

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
    : ProfileKeyedServiceFactory(
          "BorealisService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

BorealisServiceFactory::~BorealisServiceFactory() = default;

KeyedService* BorealisServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new BorealisServiceImpl(Profile::FromBrowserContext(context));
}

bool BorealisServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace borealis
