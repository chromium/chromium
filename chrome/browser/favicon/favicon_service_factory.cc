// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_service_factory.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/favicon/chrome_favicon_client.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/favicon/content/large_favicon_provider_getter.h"
#include "components/favicon/core/favicon_service_impl.h"
#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"

namespace {

favicon::LargeFaviconProvider* GetLargeFaviconProvider(
    content::BrowserContext* context) {
  return FaviconServiceFactory::GetInstance()->GetForProfile(
      Profile::FromBrowserContext(context), ServiceAccessType::EXPLICIT_ACCESS);
}

std::unique_ptr<KeyedService> BuildFaviconService(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  // |history_service| may be null, most likely because initialization failed.
  if (!history_service) {
    // This is rare enough that it's worth logging.
    LOG(WARNING) << "FaviconService not created as HistoryService is null";
    return nullptr;
  }
  return std::make_unique<favicon::FaviconServiceImpl>(
      std::make_unique<ChromeFaviconClient>(profile), history_service);
}

}  // namespace

// static
favicon::FaviconService* FaviconServiceFactory::GetForProfile(
    Profile* profile,
    ServiceAccessType sat) {
  if (!profile->IsOffTheRecord()) {
    return static_cast<favicon::FaviconService*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  } else if (sat == ServiceAccessType::EXPLICIT_ACCESS) {
    // Profile must be OffTheRecord in this case.
    return static_cast<favicon::FaviconService*>(
        GetInstance()->GetServiceForBrowserContext(
            profile->GetOriginalProfile(), true));
  }

  // Profile is OffTheRecord without access.
  NOTREACHED() << "This profile is OffTheRecord";
  return nullptr;
}

// static
FaviconServiceFactory* FaviconServiceFactory::GetInstance() {
  return base::Singleton<FaviconServiceFactory>::get();
}

// static
BrowserContextKeyedServiceFactory::TestingFactory
FaviconServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildFaviconService);
}

FaviconServiceFactory::FaviconServiceFactory()
    : ProfileKeyedServiceFactory("FaviconService") {
  DependsOn(HistoryServiceFactory::GetInstance());
  favicon::SetLargeFaviconProviderGetter(
      base::BindRepeating(&GetLargeFaviconProvider));
}

FaviconServiceFactory::~FaviconServiceFactory() = default;

KeyedService* FaviconServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return BuildFaviconService(context).release();
}

bool FaviconServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
