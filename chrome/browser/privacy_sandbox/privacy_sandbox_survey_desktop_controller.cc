// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_survey_desktop_controller.h"

#include "chrome/browser/privacy_sandbox/privacy_sandbox_survey_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"

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
    return;
  }
  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile, /*create_if_necessary=*/true);
  if (!hats_service) {
    return;
  }
  hats_service->LaunchSurvey(
      kHatsSurveyTriggerPrivacySandboxSentimentSurvey,
      /*success_callback=*/
      base::BindOnce(
          &PrivacySandboxSurveyDesktopController::OnSentimentSurveyShown,
          weak_ptr_factory_.GetWeakPtr(), profile),
      // TODO(crbug.com/346991233): Have failures emit an histogram.
      /*failure_callback=*/base::DoNothing(),
      /*product_specific_bits_data=*/survey_service_->GetSentimentSurveyPsb(),
      /*product_specific_string_data=*/{});
}

void PrivacySandboxSurveyDesktopController::OnSentimentSurveyShown(
    Profile* profile) {
  survey_service_->OnSuccessfulSentimentSurvey();
}

}  // namespace privacy_sandbox
