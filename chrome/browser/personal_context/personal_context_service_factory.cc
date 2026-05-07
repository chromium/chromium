// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/personal_context/personal_context_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/core/personal_context_service_impl.h"

// static
personal_context::PersonalContextService*
PersonalContextServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<personal_context::PersonalContextService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PersonalContextServiceFactory*
PersonalContextServiceFactory::GetInstance() {
  static base::NoDestructor<PersonalContextServiceFactory> instance;
  return instance.get();
}

PersonalContextServiceFactory::PersonalContextServiceFactory()
    : ProfileKeyedServiceFactory(
          "PersonalContextService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

PersonalContextServiceFactory::~PersonalContextServiceFactory() =
    default;

std::unique_ptr<KeyedService>
PersonalContextServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          personal_context::features::kPersonalContext)) {
    return nullptr;
  }

  return std::make_unique<
      personal_context::PersonalContextServiceImpl>();
}
