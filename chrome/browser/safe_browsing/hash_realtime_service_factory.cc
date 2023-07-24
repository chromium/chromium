// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/hash_realtime_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/network_context_service_factory.h"
#include "chrome/browser/safe_browsing/ohttp_key_service_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/verdict_cache_manager_factory.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_service.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_context.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"

namespace safe_browsing {

// static
HashRealTimeService* HashRealTimeServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<HashRealTimeService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
HashRealTimeServiceFactory* HashRealTimeServiceFactory::GetInstance() {
  static base::NoDestructor<HashRealTimeServiceFactory> instance;
  return instance.get();
}

HashRealTimeServiceFactory::HashRealTimeServiceFactory()
    : ProfileKeyedServiceFactory(
          "HashRealTimeService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(VerdictCacheManagerFactory::GetInstance());
  DependsOn(NetworkContextServiceFactory::GetInstance());
  DependsOn(OhttpKeyServiceFactory::GetInstance());
}

KeyedService* HashRealTimeServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!g_browser_process->safe_browsing_service()) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  auto url_loader_factory =
      std::make_unique<network::CrossThreadPendingSharedURLLoaderFactory>(
          g_browser_process->safe_browsing_service()->GetURLLoaderFactory(
              profile));
  return new HashRealTimeService(
      network::SharedURLLoaderFactory::Create(std::move(url_loader_factory)),
      base::BindRepeating(&HashRealTimeServiceFactory::GetNetworkContext,
                          profile),
      VerdictCacheManagerFactory::GetForProfile(profile),
      OhttpKeyServiceFactory::GetForProfile(profile),
      base::BindRepeating(
          &HashRealTimeServiceFactory::IsEnhancedProtectionEnabled, profile),
      WebUIInfoSingleton::GetInstance());
}

// static
bool HashRealTimeServiceFactory::IsEnhancedProtectionEnabled(Profile* profile) {
  return safe_browsing::IsEnhancedProtectionEnabled(*(profile->GetPrefs()));
}

// static
network::mojom::NetworkContext* HashRealTimeServiceFactory::GetNetworkContext(
    Profile* profile) {
  return g_browser_process->safe_browsing_service()->GetNetworkContext(profile);
}

}  // namespace safe_browsing
