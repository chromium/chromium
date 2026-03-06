// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/finds_service_factory.h"

#include "chrome/browser/finds/core/finds_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace finds {

// static
FindsServiceFactory* FindsServiceFactory::GetInstance() {
  static base::NoDestructor<FindsServiceFactory> instance;
  return instance.get();
}

// static
FindsService* FindsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<FindsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

FindsServiceFactory::FindsServiceFactory()
    : ProfileKeyedServiceFactory(
          "FindsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

FindsServiceFactory::~FindsServiceFactory() = default;

std::unique_ptr<KeyedService>
FindsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<FindsService>();
}

}  // namespace finds
