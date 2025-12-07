// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_data_keyed_service_factory.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/ai/ai_data_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

// static
AiDataKeyedService* AiDataKeyedServiceFactory::GetAiDataKeyedService(
    content::BrowserContext* browser_context) {
  return static_cast<AiDataKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
AiDataKeyedServiceFactory* AiDataKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<AiDataKeyedServiceFactory> factory;
  return factory.get();
}

AiDataKeyedServiceFactory::AiDataKeyedServiceFactory()
    : ProfileKeyedServiceFactory(
          "AiDataKeyedService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

AiDataKeyedServiceFactory::~AiDataKeyedServiceFactory() = default;

bool AiDataKeyedServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return false;
}

std::unique_ptr<KeyedService>
AiDataKeyedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<AiDataKeyedService>(context);
}
