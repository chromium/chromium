// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_intelligent_scan_delegate_factory.h"

#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/safe_browsing/android/client_side_detection_intelligent_scan_delegate_android.h"
#else
#include "chrome/browser/safe_browsing/client_side_detection_intelligent_scan_delegate_desktop.h"
#endif

namespace safe_browsing {

// static
ClientSideDetectionHost::IntelligentScanDelegate*
ClientSideDetectionIntelligentScanDelegateFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ClientSideDetectionHost::IntelligentScanDelegate*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
ClientSideDetectionIntelligentScanDelegateFactory*
ClientSideDetectionIntelligentScanDelegateFactory::GetInstance() {
  static base::NoDestructor<ClientSideDetectionIntelligentScanDelegateFactory>
      instance;
  return instance.get();
}

ClientSideDetectionIntelligentScanDelegateFactory::
    ClientSideDetectionIntelligentScanDelegateFactory()
    : ProfileKeyedServiceFactory(
          "IntelligentScanDelegate",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

std::unique_ptr<KeyedService>
ClientSideDetectionIntelligentScanDelegateFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<ClientSideDetectionIntelligentScanDelegateAndroid>();
#else
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<ClientSideDetectionIntelligentScanDelegateDesktop>(
      *profile->GetPrefs());
#endif
}

}  // namespace safe_browsing
