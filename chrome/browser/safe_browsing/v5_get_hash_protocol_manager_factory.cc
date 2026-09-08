// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/v5_get_hash_protocol_manager_factory.h"

#include <memory>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_v4_protocol_config_provider.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/v5_search_hashes_cache_factory.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/safe_browsing/core/browser/db/v5_get_hash_protocol_manager.h"
#include "content/public/browser/browser_context.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

// static
V5GetHashProtocolManager* V5GetHashProtocolManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<V5GetHashProtocolManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
V5GetHashProtocolManager* V5GetHashProtocolManagerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return GetForProfile(Profile::FromBrowserContext(browser_context));
}

// static
V5GetHashProtocolManagerFactory*
V5GetHashProtocolManagerFactory::GetInstance() {
  static base::NoDestructor<V5GetHashProtocolManagerFactory> instance;
  return instance.get();
}

V5GetHashProtocolManagerFactory::V5GetHashProtocolManagerFactory()
    : ProfileKeyedServiceFactory(
          "V5GetHashProtocolManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(V5SearchHashesCacheFactory::GetInstance());
}

std::unique_ptr<KeyedService>
V5GetHashProtocolManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!g_browser_process->safe_browsing_service()) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  // TODO(crbug.com/362791941): handle v4 references
  return std::make_unique<V5GetHashProtocolManager>(
      g_browser_process->shared_url_loader_factory(), GetV4ProtocolConfig(),
      V5SearchHashesCacheFactory::GetForProfile(profile));
}

}  // namespace safe_browsing
