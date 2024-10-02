// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_service_factory.h"

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_client_side_detection_service_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

// static
ClientSideDetectionService* ClientSideDetectionServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ClientSideDetectionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create= */
                                                 true));
}

// static
ClientSideDetectionServiceFactory*
ClientSideDetectionServiceFactory::GetInstance() {
  static base::NoDestructor<ClientSideDetectionServiceFactory> instance;
  return instance.get();
}

ClientSideDetectionServiceFactory::ClientSideDetectionServiceFactory()
    : ProfileKeyedServiceFactory(
          "ClientSideDetectionService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // ChromeOS creates various profiles (login, lock screen...) that
              // do not display web content and thus do not need the
              // client side phishing detection
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

std::unique_ptr<KeyedService>
ClientSideDetectionServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  auto* opt_guide = OptimizationGuideKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(context));

  if (!opt_guide) {
    return nullptr;
  }

  return std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile),
      opt_guide);
}

bool ClientSideDetectionServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool ClientSideDetectionServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace safe_browsing
