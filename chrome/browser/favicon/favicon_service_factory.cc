// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_service_factory.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "chrome/browser/favicon/chrome_favicon_client.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/favicon/core/favicon_service_impl.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"

namespace {

std::unique_ptr<KeyedService> BuildFaviconService(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<favicon::FaviconServiceImpl>(
      base::WrapUnique(new ChromeFaviconClient(profile)),
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS));
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
  return NULL;
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
    : BrowserContextKeyedServiceFactory(
        "FaviconService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HistoryServiceFactory::GetInstance());
}

FaviconServiceFactory::~FaviconServiceFactory() {
}

KeyedService* FaviconServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return BuildFaviconService(context).release();
}

bool FaviconServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
