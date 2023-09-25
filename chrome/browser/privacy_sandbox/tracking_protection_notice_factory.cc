// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/tracking_protection_notice_factory.h"
#include <memory>
#include "base/no_destructor.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_notice_service.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"
#include "chrome/browser/profiles/profile.h"

TrackingProtectionNoticeFactory*
TrackingProtectionNoticeFactory::GetInstance() {
  static base::NoDestructor<TrackingProtectionNoticeFactory> instance;
  return instance.get();
}

privacy_sandbox::TrackingProtectionNoticeService*
TrackingProtectionNoticeFactory::GetForProfile(Profile* profile) {
  return static_cast<privacy_sandbox::TrackingProtectionNoticeService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

TrackingProtectionNoticeFactory::TrackingProtectionNoticeFactory()
    : ProfileKeyedServiceFactory("TrackingProtectionNotice") {
  DependsOn(TrackingProtectionOnboardingFactory::GetInstance());
}

std::unique_ptr<KeyedService>
TrackingProtectionNoticeFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  auto* onboarding_sevice =
      TrackingProtectionOnboardingFactory::GetForProfile(profile);

  return onboarding_sevice
             ? std::make_unique<
                   privacy_sandbox::TrackingProtectionNoticeService>(
                   profile, onboarding_sevice)
             : nullptr;
}

bool TrackingProtectionNoticeFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}
