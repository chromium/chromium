// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privacy_sandbox_whats_new_survey_service.h"

#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/supports_user_data.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/webui/whats_new/whats_new.mojom-data-view.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_interaction_data.h"
#include "chrome/grit/branded_strings.h"
#include "content/public/browser/web_contents.h"
#include "privacy_sandbox_incognito_features.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

bool WasModuleShown(
    const std::vector<WhatsNewInteractionData::ModuleShown>& modules_shown,
    std::string_view module_name) {
  return std::any_of(
      modules_shown.begin(), modules_shown.end(),
      [module_name](const WhatsNewInteractionData::ModuleShown& module) {
        return module.name == module_name;
      });
}

}  // namespace

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
    content::WebContents* const web_contents) {
  if (!IsSurveyEnabled()) {
    RecordSurveyStatus(WhatsNewSurveyStatus::kFeatureDisabled);
    return;
  }

  auto delay = privacy_sandbox::kPrivacySandboxWhatsNewSurveyDelay.Get();
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &PrivacySandboxWhatsNewSurveyService::LaunchSurveyWithPsd,
          weak_ptr_factory_.GetWeakPtr(), web_contents->GetWeakPtr(),
          std::string(kHatsSurveyTriggerPrivacySandboxWhatsNewSurvey)),
      delay);
}

void PrivacySandboxWhatsNewSurveyService::OnSurveyShown() {
  RecordSurveyStatus(WhatsNewSurveyStatus::kSurveyShown);
}

void PrivacySandboxWhatsNewSurveyService::OnSurveyFailure() {
  RecordSurveyStatus(WhatsNewSurveyStatus::kSurveyLaunchFailed);
}

SurveyStringData PrivacySandboxWhatsNewSurveyService::GetPsd(
    content::WebContents* const web_contents) {
  SurveyStringData psd = {};

  WhatsNewInteractionData* interaction_data =
      WhatsNewInteractionData::FromWebContents(web_contents);

  if (interaction_data) {
    const std::vector<WhatsNewInteractionData::ModuleShown>& modules =
        interaction_data->modules_shown();
    psd[kHasSeenActFeaturesPsdKey] =
        WasModuleShown(modules,
                       privacy_sandbox::kPrivacySandboxActWhatsNew.name)
            ? "true"
            : "false";
  } else {
    // We cannot tell whether user has seen our module.
    psd[kHasSeenActFeaturesPsdKey] = "unknown";
  }

  return psd;
}

void PrivacySandboxWhatsNewSurveyService::LaunchSurveyWithPsd(
    base::WeakPtr<content::WebContents> web_contents_weak_ptr,
    const std::string& trigger) {
  content::WebContents* web_contents = web_contents_weak_ptr.get();
  if (!web_contents || web_contents->IsBeingDestroyed()) {
    RecordSurveyStatus(WhatsNewSurveyStatus::kWebContentsDestructed);
    return;
  }

  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);

  if (!hats_service) {
    RecordSurveyStatus(WhatsNewSurveyStatus::kHatsServiceFailed);
    return;
  }

  // Calculate PSD at the moment of launch

  SurveyStringData psd = GetPsd(web_contents);

  RecordSurveyStatus(WhatsNewSurveyStatus::kSurveyLaunched);
  // Launch the survey immediately with the fresh PSD
  hats_service->LaunchSurveyForWebContents(
      trigger, web_contents,
      /*product_specific_bits_data=*/{},
      /*product_specific_string_data=*/psd,
      /*success_callback=*/
      base::BindOnce(&PrivacySandboxWhatsNewSurveyService::OnSurveyShown,
                     weak_ptr_factory_.GetWeakPtr()),
      /*failure_callback=*/
      base::BindOnce(&PrivacySandboxWhatsNewSurveyService::OnSurveyFailure,
                     weak_ptr_factory_.GetWeakPtr()),
      /*supplied_trigger_id=*/std::nullopt);
}

}  // namespace privacy_sandbox
