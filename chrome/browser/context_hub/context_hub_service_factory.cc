// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/context_hub_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/context_hub/context_hub_service.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/profiles/profile.h"

// static
context_hub::ContextHubService* ContextHubServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<context_hub::ContextHubService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ContextHubServiceFactory* ContextHubServiceFactory::GetInstance() {
  static base::NoDestructor<ContextHubServiceFactory> instance;
  return instance.get();
}

ContextHubServiceFactory::ContextHubServiceFactory()
    : ProfileKeyedServiceFactory(
          "ContextHubService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

ContextHubServiceFactory::~ContextHubServiceFactory() = default;

std::unique_ptr<KeyedService>
ContextHubServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(context_hub::features::kContextHub)) {
    return nullptr;
  }
  return std::make_unique<context_hub::ContextHubService>();
}
