// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privacy_sandbox_whats_new_survey_service.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/grit/branded_strings.h"
#include "content/public/browser/web_contents.h"
#include "privacy_sandbox_incognito_features.h"
#include "ui/base/l10n/l10n_util.h"

namespace privacy_sandbox {

PrivacySandboxWhatsNewSurveyService::PrivacySandboxWhatsNewSurveyService(
    Profile* profile)
    : profile_(profile) {}

PrivacySandboxWhatsNewSurveyService::~PrivacySandboxWhatsNewSurveyService() =
    default;

bool PrivacySandboxWhatsNewSurveyService::IsSurveyEnabled() {
  return base::FeatureList::IsEnabled(
      privacy_sandbox::kPrivacySandboxWhatsNewSurvey);
}

void PrivacySandboxWhatsNewSurveyService::RecordSurveyStatus(
    WhatsNewSurveyStatus status) {
  base::UmaHistogramEnumeration("PrivacySandbox.WhatsNewSurvey.Status", status);
}

void PrivacySandboxWhatsNewSurveyService::MaybeShowSurvey(
    content::WebContents* web_contents) {
  if (!IsSurveyEnabled()) {
    RecordSurveyStatus(WhatsNewSurveyStatus::kFeatureDisabled);
    return;
  }

  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);

  if (!hats_service) {
    RecordSurveyStatus(WhatsNewSurveyStatus::kHatsServiceFailed);
    return;
  }

  auto delay = privacy_sandbox::kPrivacySandboxWhatsNewSurveyDelay.Get();
  auto delay_ms = delay.InMilliseconds();

  // record that survey was launched to detect premature exits
  RecordSurveyStatus(WhatsNewSurveyStatus::kSurveyLaunched);
  hats_service->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerPrivacySandboxWhatsNewSurvey, web_contents, delay_ms,
      /*product_specific_bits_data=*/{}, /*product_specific_string_data=*/{},
      /*navigation_behavior=*/
      HatsService::NavigationBehavior::REQUIRE_SAME_DOCUMENT,
      /*success_callback=*/
      base::BindOnce(&PrivacySandboxWhatsNewSurveyService::OnSurveyShown,
                     weak_ptr_factory_.GetWeakPtr()),
      /*failure_callback=*/
      base::BindOnce(&PrivacySandboxWhatsNewSurveyService::OnSurveyFailure,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PrivacySandboxWhatsNewSurveyService::OnSurveyShown() {
  RecordSurveyStatus(WhatsNewSurveyStatus::kSurveyShown);
}

void PrivacySandboxWhatsNewSurveyService::OnSurveyFailure() {
  RecordSurveyStatus(WhatsNewSurveyStatus::kSurveyLaunchFailed);
}

}  // namespace privacy_sandbox
