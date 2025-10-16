// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/omnibox/contextual_session_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "components/omnibox/composebox/contextual_session_service.h"
#include "components/version_info/version_info.h"

// static
ContextualSessionService* ContextualSessionServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ContextualSessionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ContextualSessionServiceFactory*
ContextualSessionServiceFactory::GetInstance() {
  static base::NoDestructor<ContextualSessionServiceFactory> instance;
  return instance.get();
}

ContextualSessionServiceFactory::ContextualSessionServiceFactory()
    : ProfileKeyedServiceFactory(
          "ContextualSessionService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

ContextualSessionServiceFactory::~ContextualSessionServiceFactory() = default;

std::unique_ptr<KeyedService>
ContextualSessionServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<ContextualSessionService>(
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetURLLoaderFactory(),
      TemplateURLServiceFactory::GetForProfile(profile),
      profile->GetVariationsClient(), chrome::GetChannel(),
      g_browser_process->GetApplicationLocale());
}
