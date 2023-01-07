// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accuracy_tips/accuracy_service_delegate.h"

#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/page_info/chrome_accuracy_tip_ui.h"
#include "components/accuracy_tips/features.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"

AccuracyServiceDelegate::~AccuracyServiceDelegate() = default;

AccuracyServiceDelegate::AccuracyServiceDelegate(Profile* profile)
    : profile_(profile) {}

bool AccuracyServiceDelegate::IsEngagementHigh(const GURL& url) {
  auto* engagement_service =
      site_engagement::SiteEngagementServiceFactory::GetForProfile(profile_);
  int max_engagement = accuracy_tips::features::kMaxSiteEngagementScore.Get();
  if (max_engagement ==
      accuracy_tips::features::kMaxSiteEngagementScore.default_value) {
    max_engagement =
        site_engagement::SiteEngagementScore::GetMediumEngagementBoundary();
  }
  return engagement_service->GetScore(url) >= max_engagement;
}

void AccuracyServiceDelegate::ShowAccuracyTip(
    content::WebContents* web_contents,
    accuracy_tips::AccuracyTipStatus type,
    bool show_opt_out,
    base::OnceCallback<void(accuracy_tips::AccuracyTipInteraction)>
        close_callback) {
  ShowAccuracyTipDialog(web_contents, type, show_opt_out,
                        std::move(close_callback));
}

void AccuracyServiceDelegate::ShowSurvey(
    const std::map<std::string, bool>& product_specific_bits_data,
    const std::map<std::string, std::string>& product_specific_string_data) {
  auto* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);
  if (!hats_service)
    return;

  hats_service->LaunchSurvey(kHatsSurveyTriggerAccuracyTips,
                             /*success_callback=*/base::DoNothing(),
                             /*failure_callback=*/base::DoNothing(),
                             product_specific_bits_data,
                             product_specific_string_data);
}

bool AccuracyServiceDelegate::IsSecureConnection(
    content::WebContents* web_contents) {
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  return helper ? helper->GetSecurityLevel() ==
                      security_state::SecurityLevel::SECURE
                : false;
}
