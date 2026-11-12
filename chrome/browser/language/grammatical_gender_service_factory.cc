// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language/grammatical_gender_service_factory.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/language/core/browser/grammatical_gender_service.h"

// static
language::GrammaticalGenderService*
GrammaticalGenderServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<language::GrammaticalGenderService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
GrammaticalGenderServiceFactory*
GrammaticalGenderServiceFactory::GetInstance() {
  static base::NoDestructor<GrammaticalGenderServiceFactory> instance;
  return instance.get();
}

GrammaticalGenderServiceFactory::GrammaticalGenderServiceFactory()
    : ProfileKeyedServiceFactory(
          "GrammaticalGenderService",
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

GrammaticalGenderServiceFactory::~GrammaticalGenderServiceFactory() = default;

std::unique_ptr<KeyedService>
GrammaticalGenderServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<language::GrammaticalGenderService>(
      IdentityManagerFactory::GetForProfile(profile));
}
