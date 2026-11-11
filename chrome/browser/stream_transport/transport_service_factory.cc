// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/stream_transport/transport_service_factory.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/browser_actuator/public/features.h"
#include "components/stream_transport/internal/transport_service_impl.h"
#include "components/stream_transport/public/transport_service.h"
#include "content/public/browser/browser_context.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace stream_transport {

// static
TransportService* TransportServiceFactory::GetForProfile(Profile* profile) {
  CHECK(profile);
  return static_cast<TransportService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
TransportServiceFactory* TransportServiceFactory::GetInstance() {
  static base::NoDestructor<TransportServiceFactory> instance;
  return instance.get();
}

TransportServiceFactory::TransportServiceFactory()
    : ProfileKeyedServiceFactory("BrowserActuatorTransportService",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

TransportServiceFactory::~TransportServiceFactory() = default;

std::unique_ptr<KeyedService>
TransportServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // TODO(crbug.com/568448841): Browser actuation is the transport's only
  // client so far, so its feature gates the transport. Give the transport its
  // own feature once a second client arrives.
  if (!base::FeatureList::IsEnabled(browser_actuator::kBrowserActuator)) {
    return nullptr;
  }

  TransportServiceConfig config;
  config.watch_sessions_endpoint = browser_actuator::GetWatchSessionsEndPoint();
  config.send_session_message_endpoint =
      browser_actuator::GetSendSessionMessageEndpoint();
  config.record_sessions =
      base::FeatureList::IsEnabled(browser_actuator::kBrowserActuatorInternals);

  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<TransportServiceImpl>(
      context->GetURLLoaderFactory(),
      IdentityManagerFactory::GetForProfile(profile), std::move(config));
}

}  // namespace stream_transport
