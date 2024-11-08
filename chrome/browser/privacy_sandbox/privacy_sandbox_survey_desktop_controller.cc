// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_survey_desktop_controller.h"

#include "chrome/browser/privacy_sandbox/privacy_sandbox_survey_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "components/privacy_sandbox/privacy_sandbox_survey_service.h"

namespace privacy_sandbox {

PrivacySandboxSurveyDesktopController::PrivacySandboxSurveyDesktopController(
    PrivacySandboxSurveyService* survey_service)
    : survey_service_(survey_service) {
  CHECK(survey_service_);
}
PrivacySandboxSurveyDesktopController::
    ~PrivacySandboxSurveyDesktopController() = default;

void PrivacySandboxSurveyDesktopController::MaybeShowSentimentSurvey(
    Profile* profile) {
  if (!survey_service_->ShouldShowSentimentSurvey()) {
    survey_service_->RecordSentimentSurveyStatus(
        PrivacySandboxSurveyService::PrivacySandboxSentimentSurveyStatus::
            kFeatureDisabled);
    return;
  }
  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile,
                                        /*create_if_necessary=*/true);
  if (!hats_service) {
    survey_service_->RecordSentimentSurveyStatus(
        PrivacySandboxSurveyService::PrivacySandboxSentimentSurveyStatus::
            kHatsServiceFailed);
    return;
  }
  hats_service->LaunchSurvey(
      kHatsSurveyTriggerPrivacySandboxSentimentSurvey,
      /*success_callback=*/
      base::BindOnce(
          &PrivacySandboxSurveyDesktopController::OnSentimentSurveyShown,
          weak_ptr_factory_.GetWeakPtr(), profile),
      /*failure_callback=*/
      base::BindOnce(
          &PrivacySandboxSurveyDesktopController::OnSentimentSurveyFailure,
          weak_ptr_factory_.GetWeakPtr()),
      /*product_specific_bits_data=*/survey_service_->GetSentimentSurveyPsb(),
      /*product_specific_string_data=*/{});
}

void PrivacySandboxSurveyDesktopController::OnSentimentSurveyShown(
    Profile* profile) {
  survey_service_->RecordSentimentSurveyStatus(
      PrivacySandboxSurveyService::PrivacySandboxSentimentSurveyStatus::
          kSurveyShown);
}

void PrivacySandboxSurveyDesktopController::OnSentimentSurveyFailure() {
  survey_service_->RecordSentimentSurveyStatus(
      PrivacySandboxSurveyService::PrivacySandboxSentimentSurveyStatus::
          kSurveyLaunchFailed);
}

}  // namespace privacy_sandbox
