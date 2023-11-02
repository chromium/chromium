// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_service_factory.h"

#include "base/command_line.h"
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
  return base::Singleton<ClientSideDetectionServiceFactory>::get();
}

ClientSideDetectionServiceFactory::ClientSideDetectionServiceFactory()
    : ProfileKeyedServiceFactory("ClientSideDetectionService") {}

KeyedService* ClientSideDetectionServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new ClientSideDetectionService(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile));
}

}  // namespace safe_browsing
