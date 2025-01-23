// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_info/merchant_trust_service_delegate.h"

#include "chrome/browser/page_info/page_info_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "components/page_info/core/features.h"

MerchantTrustServiceDelegate::MerchantTrustServiceDelegate(Profile* profile)
    : profile_(profile) {}

MerchantTrustServiceDelegate::~MerchantTrustServiceDelegate() = default;

void MerchantTrustServiceDelegate::ShowEvaluationSurvey() {
  auto* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);
  if (!hats_service) {
    return;
  }

  if (page_info::IsMerchantTrustFeatureEnabled()) {
    CHECK(base::FeatureList::IsEnabled(
        page_info::kMerchantTrustEvaluationExperimentSurvey));
    hats_service->LaunchSurvey(
        kHatsSurveyTriggerMerchantTrustEvaluationExperimentSurvey);
  } else {
    CHECK(base::FeatureList::IsEnabled(
        page_info::kMerchantTrustEvaluationControlSurvey));
    hats_service->LaunchSurvey(
        kHatsSurveyTriggerMerchantTrustEvaluationControlSurvey);
  }
}

double MerchantTrustServiceDelegate::GetSiteEngagementScore(const GURL url) {
  auto* site_engagement_service =
      site_engagement::SiteEngagementServiceFactory::GetForProfile(profile_);
  if (!site_engagement_service) {
    return 0.0;
  }
  return site_engagement_service->GetScore(url);
}
