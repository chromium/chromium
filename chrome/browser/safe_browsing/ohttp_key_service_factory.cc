// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/ohttp_key_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/network_context_service_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/ohttp_key_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_context.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"

namespace safe_browsing {

// static
OhttpKeyService* OhttpKeyServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<OhttpKeyService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
OhttpKeyServiceFactory* OhttpKeyServiceFactory::GetInstance() {
  return base::Singleton<OhttpKeyServiceFactory>::get();
}

OhttpKeyServiceFactory::OhttpKeyServiceFactory()
    : ProfileKeyedServiceFactory(
          "SafeBrowsingOhttpKeyService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(NetworkContextServiceFactory::GetInstance());
}

KeyedService* OhttpKeyServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!g_browser_process->safe_browsing_service()) {
    return nullptr;
  }
  if (!base::FeatureList::IsEnabled(kHashRealTimeOverOhttp)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  auto url_loader_factory =
      std::make_unique<network::CrossThreadPendingSharedURLLoaderFactory>(
          g_browser_process->safe_browsing_service()->GetURLLoaderFactory(
              profile));
  return new OhttpKeyService(
      network::SharedURLLoaderFactory::Create(std::move(url_loader_factory)),
      profile->GetPrefs());
}

bool OhttpKeyServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // The service is created early to start async key fetch.
  return true;
}

}  // namespace safe_browsing
