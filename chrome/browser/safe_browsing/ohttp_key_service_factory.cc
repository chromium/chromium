// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/ohttp_key_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/network_context_service_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/ohttp_key_service.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "content/public/browser/browser_context.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"

namespace safe_browsing {
bool kAllowInTests = false;

// static
OhttpKeyService* OhttpKeyServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<OhttpKeyService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
OhttpKeyServiceFactory* OhttpKeyServiceFactory::GetInstance() {
  static base::NoDestructor<OhttpKeyServiceFactory> instance;
  return instance.get();
}

OhttpKeyServiceFactory::OhttpKeyServiceFactory()
    : ProfileKeyedServiceFactory(
          "SafeBrowsingOhttpKeyService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(NetworkContextServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
OhttpKeyServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // TODO(crbug.com/40910088) [Also TODO(thefrog)]: For now we simply return
  // nullptr for Android. If it becomes settled that Android should not use this
  // service, this will be refactored to avoid including this and associated
  // files in the binary in the first place.
#if BUILDFLAG(IS_ANDROID)
  return nullptr;
#else
  if (!g_browser_process->safe_browsing_service()) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  auto url_loader_factory =
      std::make_unique<network::CrossThreadPendingSharedURLLoaderFactory>(
          g_browser_process->safe_browsing_service()->GetURLLoaderFactory(
              profile));
  return std::make_unique<OhttpKeyService>(
      network::SharedURLLoaderFactory::Create(std::move(url_loader_factory)),
      profile->GetPrefs(), g_browser_process->local_state(),
      base::BindRepeating(&OhttpKeyServiceFactory::GetCountry));
#endif
}

bool OhttpKeyServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // The service is created early to start async key fetch.
  return true;
}

bool OhttpKeyServiceFactory::ServiceIsNULLWhileTesting() const {
  return !kAllowInTests;
}

OhttpKeyServiceAllowerForTesting::OhttpKeyServiceAllowerForTesting() {
  kAllowInTests = true;
}
OhttpKeyServiceAllowerForTesting::~OhttpKeyServiceAllowerForTesting() {
  kAllowInTests = false;
}

// static
std::optional<std::string> OhttpKeyServiceFactory::GetCountry() {
  return safe_browsing::hash_realtime_utils::GetCountryCode(
      g_browser_process->variations_service());
}

}  // namespace safe_browsing
