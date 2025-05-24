// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_survey_desktop_controller.h"

#include "base/metrics/histogram_functions.h"
#include "base/version_info/channel.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_survey_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace privacy_sandbox {

namespace {

using SentimentSurveyStatus = ::privacy_sandbox::PrivacySandboxSurveyService::
    PrivacySandboxSentimentSurveyStatus;
using enum SentimentSurveyStatus;

bool ShouldShowSentimentSurvey() {
  return base::FeatureList::IsEnabled(kPrivacySandboxSentimentSurvey);
}

void RecordSentimentSurveyStatus(SentimentSurveyStatus status) {
  base::UmaHistogramEnumeration("PrivacySandbox.SentimentSurvey.Status",
                                status);
}

std::map<std::string, bool> GetSentimentSurveyPsb(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager) {
  CHECK(pref_service);
  CHECK(identity_manager);
  return {
      {"Topics enabled",
       pref_service->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled)},
      {"Protected audience enabled",
       pref_service->GetBoolean(prefs::kPrivacySandboxM1FledgeEnabled)},
      {"Measurement enabled",
       pref_service->GetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled)},
      {"Signed in",
       identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)}};
}

std::map<std::string, std::string> GetSentimentSurveyPsd() {
  return {{"Channel",
           std::string(version_info::GetChannelString(chrome::GetChannel()))}};
}

}  // namespace

PrivacySandboxSurveyDesktopController::PrivacySandboxSurveyDesktopController(
    Profile* profile)
    : profile_(profile) {
  CHECK(profile_);
}

PrivacySandboxSurveyDesktopController::
    ~PrivacySandboxSurveyDesktopController() = default;

void PrivacySandboxSurveyDesktopController::MaybeShowSentimentSurvey() {
  // TODO(kjarosz) Add check for Profile == Regular when the profile selection
  // rule include OTR.

  if (!has_seen_ntp_) {
    return;
  }
  if (!ShouldShowSentimentSurvey()) {
    RecordSentimentSurveyStatus(kFeatureDisabled);
    return;
  }
  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_,
                                        /*create_if_necessary=*/true);
  if (!hats_service) {
    RecordSentimentSurveyStatus(kHatsServiceFailed);
    return;
  }

  hats_service->LaunchSurvey(
      kHatsSurveyTriggerPrivacySandboxSentimentSurvey,
      /*success_callback=*/
      base::BindOnce(
          &PrivacySandboxSurveyDesktopController::OnSentimentSurveyShown,
          weak_ptr_factory_.GetWeakPtr()),
      /*failure_callback=*/
      base::BindOnce(
          &PrivacySandboxSurveyDesktopController::OnSentimentSurveyFailure,
          weak_ptr_factory_.GetWeakPtr()),
      GetSentimentSurveyPsb(profile_->GetPrefs(),
                            IdentityManagerFactory::GetForProfile(profile_)),
      /*product_specific_string_data=*/
      GetSentimentSurveyPsd());
}

void PrivacySandboxSurveyDesktopController::OnNewTabPageSeen() {
  has_seen_ntp_ = true;
}

void PrivacySandboxSurveyDesktopController::OnSentimentSurveyShown() {
  RecordSentimentSurveyStatus(kSurveyShown);
}

void PrivacySandboxSurveyDesktopController::OnSentimentSurveyFailure() {
  RecordSentimentSurveyStatus(kSurveyLaunchFailed);
}

}  // namespace privacy_sandbox
