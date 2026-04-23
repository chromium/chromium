// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/hats/auto_picture_in_picture_hats_service.h"

#include "base/functional/callback_helpers.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"

namespace {

using AutoPipReason = media::PictureInPictureEventsInfo::AutoPipReason;
using PromptResult = AutoPipSettingHelper::PromptResult;

constexpr base::FeatureParam<AutoPipReason>::Option kAutoPipReasonOptions[] = {
    {AutoPipReason::kUnknown, "Unknown"},
    {AutoPipReason::kVideoConferencing, "VideoConferencing"},
    {AutoPipReason::kMediaPlayback, "MediaPlayback"},
    {AutoPipReason::kBrowserInitiated, "BrowserInitiated"}};

constexpr base::FeatureParam<PromptResult>::Option kPromptResultOptions[] = {
    {PromptResult::kIgnored, "Ignored"},
    {PromptResult::kBlock, "Block"},
    {PromptResult::kAllowOnEveryVisit, "AllowOnEveryVisit"},
    {PromptResult::kAllowOnce, "AllowOnce"},
    {PromptResult::kNotShownAllowedOnEveryVisit, "NotShownAllowedOnEveryVisit"},
    {PromptResult::kNotShownAllowedOnce, "NotShownAllowedOnce"},
    {PromptResult::kNotShownBlocked, "NotShownBlocked"},
    {PromptResult::kNotShownIncognito, "NotShownIncognito"}};

std::string AutoPipReasonToString(AutoPipReason reason) {
  switch (reason) {
    case AutoPipReason::kUnknown:
      return "Unknown";
    case AutoPipReason::kVideoConferencing:
      return "VideoConferencing";
    case AutoPipReason::kMediaPlayback:
      return "MediaPlayback";
    case AutoPipReason::kBrowserInitiated:
      return "BrowserInitiated";
  }
}

std::string PromptResultToString(PromptResult result) {
  switch (result) {
    case PromptResult::kIgnored:
      return "Ignored";
    case PromptResult::kBlock:
      return "Block";
    case PromptResult::kAllowOnEveryVisit:
      return "AllowOnEveryVisit";
    case PromptResult::kAllowOnce:
      return "AllowOnce";
    case PromptResult::kNotShownAllowedOnEveryVisit:
      return "NotShownAllowedOnEveryVisit";
    case PromptResult::kNotShownAllowedOnce:
      return "NotShownAllowedOnce";
    case PromptResult::kNotShownBlocked:
      return "NotShownBlocked";
    case PromptResult::kNotShownIncognito:
      return "NotShownIncognito";
  }
}

// Helper to map a PromptResult to its corresponding HaTS trigger group.
std::string GetSurveyTrigger(PromptResult result) {
  switch (result) {
    case PromptResult::kIgnored:
      return kHatsSurveyTriggerAutoPipPermissionPromptIgnored;
    case PromptResult::kBlock:
    case PromptResult::kNotShownBlocked:
    case PromptResult::kNotShownIncognito:
      return kHatsSurveyTriggerAutoPipBlocked;
    case PromptResult::kAllowOnEveryVisit:
    case PromptResult::kAllowOnce:
    case PromptResult::kNotShownAllowedOnEveryVisit:
    case PromptResult::kNotShownAllowedOnce:
      return kHatsSurveyTriggerAutoPipAllowed;
  }
}

}  // namespace

AutoPictureInPictureHatsService::AutoPictureInPictureHatsService(
    Profile* profile)
    : profile_(profile), clock_(base::DefaultTickClock::GetInstance()) {}

AutoPictureInPictureHatsService::~AutoPictureInPictureHatsService() = default;

void AutoPictureInPictureHatsService::AutoPictureInPictureWindowOpened(
    media::PictureInPictureEventsInfo::AutoPipReason reason,
    const GURL& origin) {
  active_window_context_ =
      ActiveWindowContext(reason, origin, clock_->NowTicks());
}

void AutoPictureInPictureHatsService::SetPromptResult(
    AutoPipSettingHelper::PromptResult result) {
  if (active_window_context_) {
    active_window_context_->prompt_result = result;
  }
}

void AutoPictureInPictureHatsService::AutoPictureInPictureWindowClosed() {
  if (!active_window_context_) {
    return;
  }

  if (active_window_context_->window_duration) {
    return;
  }

  active_window_context_->window_duration =
      clock_->NowTicks() - active_window_context_->start_time;
}

void AutoPictureInPictureHatsService::MaybeLaunchSurvey(
    content::WebContents* web_contents) {
  if (!active_window_context_) {
    return;
  }

  // If the window is still open, we should not clear the context yet.
  if (!active_window_context_->window_duration) {
    return;
  }

  // If the window is closed but we never got a prompt result, we cannot launch
  // a survey. Clear context and return.
  if (!active_window_context_->prompt_result) {
    active_window_context_ = std::nullopt;
    return;
  }

  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);
  if (!hats_service) {
    active_window_context_ = std::nullopt;
    return;
  }

  AutoPipReason auto_pip_trigger_reason = active_window_context_->reason;
  PromptResult permission_prompt_result =
      *active_window_context_->prompt_result;

  // We do not trigger surveys for browser-initiated AutoPip.
  if (auto_pip_trigger_reason == AutoPipReason::kBrowserInitiated) {
    active_window_context_ = std::nullopt;
    return;
  }

  // Only trigger if reason and trigger group match the Finch-configured
  // segments.
  const std::string actual_trigger = GetSurveyTrigger(permission_prompt_result);
  const std::string target_trigger =
      GetSurveyTrigger(GetSurveyTargetPromptResult());

  if (auto_pip_trigger_reason != GetSurveyTargetReason() ||
      actual_trigger != target_trigger) {
    active_window_context_ = std::nullopt;
    return;
  }

  SurveyStringData product_specific_string_data;
  product_specific_string_data["AutoPip Reason"] =
      AutoPipReasonToString(auto_pip_trigger_reason);

  product_specific_string_data["Pip window duration"] =
      base::NumberToString(
          active_window_context_->window_duration->InSeconds()) +
      "s";

  // Record Opener site URL only if UKM is enabled for this profile.
  const bool is_ukm_enabled = profile_->GetPrefs()->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
  if (is_ukm_enabled) {
    product_specific_string_data["Opener site URL"] =
        active_window_context_->origin.spec();
  } else {
    product_specific_string_data["Opener site URL"] = "";
  }

  if (actual_trigger == kHatsSurveyTriggerAutoPipAllowed) {
    product_specific_string_data["Prompt Result"] =
        PromptResultToString(permission_prompt_result);
  }

  hats_service->LaunchSurveyForWebContents(actual_trigger, web_contents, {},
                                           product_specific_string_data);

  // Clear the context after a successful launch.
  active_window_context_ = std::nullopt;
}

media::PictureInPictureEventsInfo::AutoPipReason
AutoPictureInPictureHatsService::GetSurveyTargetReason() const {
  return base::FeatureParam<AutoPipReason>(
             &media::kAutoPictureInPictureSurveys, "autopip_reason",
             AutoPipReason::kUnknown, &kAutoPipReasonOptions)
      .Get();
}

AutoPipSettingHelper::PromptResult
AutoPictureInPictureHatsService::GetSurveyTargetPromptResult() const {
  return base::FeatureParam<PromptResult>(
             &media::kAutoPictureInPictureSurveys, "prompt_result",
             PromptResult::kIgnored, &kPromptResultOptions)
      .Get();
}

AutoPictureInPictureHatsService::ActiveWindowContext::ActiveWindowContext(
    media::PictureInPictureEventsInfo::AutoPipReason reason,
    const GURL& origin,
    base::TimeTicks start_time)
    : reason(reason), origin(origin), start_time(start_time) {}
