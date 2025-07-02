// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/ml_log_router_factory.h"

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/autofill/core/browser/ml_model/logging/ml_log_router.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace autofill {

// static
MLLogRouterFactory* MLLogRouterFactory::GetInstance() {
  static base::NoDestructor<MLLogRouterFactory> instance;
  return instance.get();
}

// static
autofill::MLLogRouter* MLLogRouterFactory::GetForProfile(Profile* profile) {
  return static_cast<autofill::MLLogRouter*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

MLLogRouterFactory::MLLogRouterFactory()
    : ProfileKeyedServiceFactory(
          "MLLogRouter",
          ProfileSelections::Builder()
              // Also provide a log router for guest profiles.
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              .Build()) {}

MLLogRouterFactory::~MLLogRouterFactory() = default;

std::unique_ptr<KeyedService>
MLLogRouterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<autofill::MLLogRouter>();
}

}  // namespace autofill
