// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fcm/fcm_service_factory.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/fcm/fcm_service.h"
#include "components/fcm/features.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/fcm/fcm_driver_android.h"
#endif

// static
fcm::FcmService* FcmServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<fcm::FcmService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
FcmServiceFactory* FcmServiceFactory::GetInstance() {
  static base::NoDestructor<FcmServiceFactory> instance;
  return instance.get();
}

FcmServiceFactory::FcmServiceFactory()
    : ProfileKeyedServiceFactory("FcmService",
                                 ProfileSelections::BuildForRegularProfile()) {}

FcmServiceFactory::~FcmServiceFactory() = default;

std::unique_ptr<KeyedService>
FcmServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(fcm::kUseFcmService)) {
    return nullptr;
  }

#if BUILDFLAG(IS_ANDROID)
  // On Android, FCM messages and Installation IDs are managed through the
  // platform Firebase SDK via FcmDriverAndroid.
  auto driver = std::make_unique<fcm::FcmDriverAndroid>();
  return std::make_unique<fcm::FcmService>(std::move(driver));
#else
  // TODO(crbug.com/545114635): Implement FCM service for Desktop.
  return nullptr;
#endif
}
