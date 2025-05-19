// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privacy_sandbox_incognito_survey_service.h"

#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "privacy_sandbox_incognito_features.h"

namespace privacy_sandbox {

PrivacySandboxIncognitoSurveyService::PrivacySandboxIncognitoSurveyService(
    HatsService* hats_service,
    const bool is_incognito)
    : rand_int_callback_(base::BindRepeating(&base::RandInt)),
      is_incognito_(is_incognito),
      hats_service_(hats_service) {
  CHECK(rand_int_callback_);
}

PrivacySandboxIncognitoSurveyService::~PrivacySandboxIncognitoSurveyService() =
    default;

void PrivacySandboxIncognitoSurveyService::SetRandIntCallbackForTesting(
    const PrivacySandboxIncognitoSurveyService::RandIntCallback&&
        rand_int_callback) {
  rand_int_callback_ = std::move(rand_int_callback);
}

bool PrivacySandboxIncognitoSurveyService::IsActSurveyEnabled() {
  return base::FeatureList::IsEnabled(
      privacy_sandbox::kPrivacySandboxActSurvey);
}

base::TimeDelta
PrivacySandboxIncognitoSurveyService::CalculateActSurveyDelay() {
  auto randomize_delay =
      privacy_sandbox::kPrivacySandboxActSurveyDelayRandomize.Get();
  if (randomize_delay) {
    base::TimeDelta min =
        privacy_sandbox::kPrivacySandboxActSurveyDelayMin.Get();
    base::TimeDelta max =
        privacy_sandbox::kPrivacySandboxActSurveyDelayMax.Get();

    if (max < min) {
      // Invalid configuration, do our best to fix it
      std::swap(min, max);
    }

    return base::Seconds(
        rand_int_callback_.Run(min.InSeconds(), max.InSeconds()));
  } else {
    return privacy_sandbox::kPrivacySandboxActSurveyDelay.Get();
  }
}

std::map<std::string, std::string>
PrivacySandboxIncognitoSurveyService::GetActSurveyPsd(int delay_ms) {
  return {{"Survey Trigger Delay", base::NumberToString(delay_ms)}};
}

void PrivacySandboxIncognitoSurveyService::RecordActSurveyStatus(
    ActSurveyStatus status) {
  base::UmaHistogramEnumeration("PrivacySandbox.ActSurvey.Status", status);
}

void PrivacySandboxIncognitoSurveyService::MaybeShowActSurvey(
    content::WebContents* web_contents) {
  if (!is_incognito_) {
    RecordActSurveyStatus(ActSurveyStatus::kNonIncognitoProfile);
    return;
  }

  if (!IsActSurveyEnabled()) {
    RecordActSurveyStatus(ActSurveyStatus::kFeatureDisabled);
    return;
  }

  if (!hats_service_) {
    RecordActSurveyStatus(ActSurveyStatus::kHatsServiceFailed);
    return;
  }

  auto delay = CalculateActSurveyDelay();
  auto delay_ms = delay.InMilliseconds();
  hats_service_->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerPrivacySandboxActSurvey, web_contents, delay_ms,
      /*product_specific_bits_data=*/{},
      /*product_specific_string_data=*/
      GetActSurveyPsd(delay_ms),
      /*navigation_behaviour=*/
      HatsService::NavigationBehaviour::REQUIRE_SAME_DOCUMENT,
      /*success_callback=*/
      base::BindOnce(&PrivacySandboxIncognitoSurveyService::OnActSurveyShown,
                     weak_ptr_factory_.GetWeakPtr()),
      /*failure_callback=*/
      base::BindOnce(&PrivacySandboxIncognitoSurveyService::OnActSurveyFailure,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PrivacySandboxIncognitoSurveyService::OnActSurveyShown() {
  RecordActSurveyStatus(ActSurveyStatus::kSurveyShown);
}

void PrivacySandboxIncognitoSurveyService::OnActSurveyFailure() {
  RecordActSurveyStatus(ActSurveyStatus::kSurveyLaunchFailed);
}

}  // namespace privacy_sandbox
