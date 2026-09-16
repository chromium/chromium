// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tips/tips_service_factory.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/tips/core/tips_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/tips/features/enhanced_safe_browsing_tip.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace tips {

// static
TipsServiceFactory* TipsServiceFactory::GetInstance() {
  static base::NoDestructor<TipsServiceFactory> instance;
  return instance.get();
}

// static
TipsService* TipsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<TipsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

TipsServiceFactory::TipsServiceFactory()
    : ProfileKeyedServiceFactory(
          "TipsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(
      segmentation_platform::SegmentationPlatformServiceFactory::GetInstance());
}

TipsServiceFactory::~TipsServiceFactory() = default;

std::unique_ptr<KeyedService>
TipsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  auto service = std::make_unique<TipsService>(
      profile->GetPrefs(),
      segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
          profile));
#if BUILDFLAG(IS_ANDROID)
  service->RegisterFeature(std::make_unique<EnhancedSafeBrowsingTip>());
#endif  // BUILDFLAG(IS_ANDROID)
  return service;
}

}  // namespace tips
