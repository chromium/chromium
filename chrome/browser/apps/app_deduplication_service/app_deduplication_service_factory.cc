// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/apps/app_deduplication_service/app_deduplication_service.h"
#include "chrome/browser/apps/app_deduplication_service/app_deduplication_service_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace {
static constexpr const char* kAppDeduplicationService =
    "AppDeduplicationService";
}  // namespace

namespace apps::deduplication {

// static
AppDeduplicationService* AppDeduplicationServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AppDeduplicationService*>(
      AppDeduplicationServiceFactory::GetInstance()
          ->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
AppDeduplicationServiceFactory* AppDeduplicationServiceFactory::GetInstance() {
  static base::NoDestructor<AppDeduplicationServiceFactory> instance;
  return instance.get();
}

// static
// The availability of deduplication service follows the availability of App
// Service in ash, because if we cannot install apps, there is no point for
// app deduplication.
bool AppDeduplicationServiceFactory::
    IsAppDeduplicationServiceAvailableForProfile(Profile* profile) {
  if (!base::FeatureList::IsEnabled(features::kAppDeduplicationService) &&
      !base::FeatureList::IsEnabled(features::kAppDeduplicationServiceFondue)) {
    return false;
  }
  // These two feature flags should not be on at the same time otherwise the
  // deduplication data may be overwritten.
  if (base::FeatureList::IsEnabled(features::kAppDeduplicationService) &&
      base::FeatureList::IsEnabled(features::kAppDeduplicationServiceFondue)) {
    return false;
  }
  return AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile);
}

AppDeduplicationServiceFactory::AppDeduplicationServiceFactory()
    : BrowserContextKeyedServiceFactory(
          kAppDeduplicationService,
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
}

AppDeduplicationServiceFactory::~AppDeduplicationServiceFactory() = default;

KeyedService* AppDeduplicationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* const profile = Profile::FromBrowserContext(context);
  if (!IsAppDeduplicationServiceAvailableForProfile(profile)) {
    return nullptr;
  }
  return new AppDeduplicationService(profile);
}

content::BrowserContext* AppDeduplicationServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  Profile* const profile = Profile::FromBrowserContext(context);
  if (!IsAppDeduplicationServiceAvailableForProfile(profile)) {
    return nullptr;
  }
  if (profile->IsGuestSession() && profile->IsOffTheRecord()) {
    return chrome::GetBrowserContextOwnInstanceInIncognito(context);
  }
  return BrowserContextKeyedServiceFactory::GetBrowserContextToUse(context);
}

bool AppDeduplicationServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace apps::deduplication
