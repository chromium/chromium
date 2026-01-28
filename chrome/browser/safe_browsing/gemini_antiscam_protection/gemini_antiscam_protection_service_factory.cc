// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/gemini_antiscam_protection/gemini_antiscam_protection_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/gemini_antiscam_protection/gemini_antiscam_protection_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_context.h"

namespace safe_browsing {

// static
GeminiAntiscamProtectionService*
GeminiAntiscamProtectionServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<GeminiAntiscamProtectionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create= */ true));
}

// static
GeminiAntiscamProtectionServiceFactory*
GeminiAntiscamProtectionServiceFactory::GetInstance() {
  static base::NoDestructor<GeminiAntiscamProtectionServiceFactory> instance;
  return instance.get();
}

GeminiAntiscamProtectionServiceFactory::GeminiAntiscamProtectionServiceFactory()
    : ProfileKeyedServiceFactory(
          "GeminiAntiscamProtectionService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .Build()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
GeminiAntiscamProtectionServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  // This feature is only available for ESB users who have the gemini anti-scam
  // protection feature enabled.
  if (!safe_browsing::IsEnhancedProtectionEnabled(*profile->GetPrefs()) ||
      !base::FeatureList::IsEnabled(
          safe_browsing::kGeminiAntiscamProtectionForMetricsCollection)) {
    return nullptr;
  }

  // Exclude enterprise users, who have a managed safe browsing policy.
  if (safe_browsing::IsSafeBrowsingPolicyManaged(*profile->GetPrefs())) {
    return nullptr;
  }

  // The optimization guide keyed service must be instantiated.
  auto* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!optimization_guide_keyed_service) {
    return nullptr;
  }

  // The history service must be instantiated.
  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  if (!history_service) {
    return nullptr;
  }

  return std::make_unique<GeminiAntiscamProtectionService>(
      optimization_guide_keyed_service, history_service);
}

}  // namespace safe_browsing
