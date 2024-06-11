// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/app_preload_service_factory.h"

#include "chrome/browser/apps/app_preload_service/app_preload_service.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/user_type_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "google_apis/google_api_keys.h"

namespace {
bool g_skip_api_key_check = false;
}  // namespace

namespace apps {

AppPreloadServiceFactory::AppPreloadServiceFactory()
    : ProfileKeyedServiceFactory(
          "AppPreloadService",
          // Service is currently only available in non-OTR regular profiles.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(web_app::WebAppProviderFactory::GetInstance());
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
}

AppPreloadServiceFactory::~AppPreloadServiceFactory() = default;

// static
AppPreloadService* AppPreloadServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<AppPreloadService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AppPreloadServiceFactory* AppPreloadServiceFactory::GetInstance() {
  static base::NoDestructor<AppPreloadServiceFactory> instance;
  return instance.get();
}

// static
bool AppPreloadServiceFactory::IsAvailable(Profile* profile) {
  if (!base::FeatureList::IsEnabled(features::kAppPreloadService)) {
    return false;
  }

  // Ensure that the build uses the Google-internal file containing the
  // official API keys, which are required to make queries to the Almanac.
  if (!google_apis::IsGoogleChromeAPIKeyUsed() && !g_skip_api_key_check) {
    return false;
  }

  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return false;
  }

  if (!web_app::AreWebAppsEnabled(profile)) {
    return false;
  }

  // App Preload Service is only available for unmanaged, unsupervised accounts
  // if AppPreloadServiceAllUserTypes is not enabled.
  if (!base::FeatureList::IsEnabled(kAppPreloadServiceAllUserTypes)) {
    std::string user_type = apps::DetermineUserType(profile);
    if (user_type != apps::kUserTypeUnmanaged) {
      return false;
    }
  }

  return true;
}

// static
void AppPreloadServiceFactory::SkipApiKeyCheckForTesting(
    bool skip_api_key_check) {
  g_skip_api_key_check = skip_api_key_check;
}

std::unique_ptr<KeyedService>
AppPreloadServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!IsAvailable(profile)) {
    return nullptr;
  }
  return std::make_unique<AppPreloadService>(profile);
}

bool AppPreloadServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace apps
