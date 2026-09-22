// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/browser_actuator_service_factory.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_actuator/internals/session_stream_recorder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/browser_actuator/internal/browser_actuator_service_impl.h"
#include "components/browser_actuator/public/browser_actuator_service.h"
#include "components/browser_actuator/public/features.h"
#include "components/browser_actuator/public/transport_handler_factory.h"
#include "content/public/browser/browser_context.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

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
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

BrowserActuatorServiceFactory::~BrowserActuatorServiceFactory() = default;

std::unique_ptr<KeyedService>
BrowserActuatorServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(kBrowserActuator)) {
    return nullptr;
  }

  // The internals page is the only consumer of the recorded session history,
  // so the recorder is created only when that page is enabled. This keeps the
  // memory cost at zero for regular users.
  std::vector<std::unique_ptr<TransportHandlerFactory>> extra_factories;
  if (base::FeatureList::IsEnabled(kBrowserActuatorInternals)) {
    extra_factories.push_back(std::make_unique<SessionStreamRecorderFactory>());
  }

  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<BrowserActuatorServiceImpl>(
      context->GetURLLoaderFactory(),
      IdentityManagerFactory::GetForProfile(profile),
      std::move(extra_factories));
}

}  // namespace browser_actuator
