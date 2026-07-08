// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/browser_actuator_service_factory.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_actuator/internal/browser_actuator_service_impl.h"
#include "components/browser_actuator/public/browser_actuator_service.h"
#include "components/browser_actuator/public/features.h"
#include "content/public/browser/browser_context.h"

namespace browser_actuator {

// static
BrowserActuatorServiceFactory* BrowserActuatorServiceFactory::GetInstance() {
  static base::NoDestructor<BrowserActuatorServiceFactory> instance;
  return instance.get();
}

// static
BrowserActuatorService* BrowserActuatorServiceFactory::GetForProfile(
    Profile* profile) {
  CHECK(profile);
  return static_cast<BrowserActuatorService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

BrowserActuatorServiceFactory::BrowserActuatorServiceFactory()
    : ProfileKeyedServiceFactory("BrowserActuatorService",
                                 ProfileSelections::BuildForRegularProfile()) {}

BrowserActuatorServiceFactory::~BrowserActuatorServiceFactory() = default;

std::unique_ptr<KeyedService>
BrowserActuatorServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(kBrowserActuator)) {
    return nullptr;
  }

  return std::make_unique<BrowserActuatorServiceImpl>();
}

}  // namespace browser_actuator
