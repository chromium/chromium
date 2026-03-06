// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/hats/auto_picture_in_picture_hats_service.h"

#include "base/metrics/field_trial_params.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "media/base/media_switches.h"

namespace {

using AutoPipReason = media::PictureInPictureEventsInfo::AutoPipReason;
using PromptResult = AutoPipSettingHelper::PromptResult;

constexpr base::FeatureParam<AutoPipReason>::Option kAutoPipReasonOptions[] = {
    {AutoPipReason::kUnknown, "Unknown"},
    {AutoPipReason::kVideoConferencing, "VideoConferencing"},
    {AutoPipReason::kMediaPlayback, "MediaPlayback"},
    {AutoPipReason::kBrowserInitiated, "BrowserInitiated"}};

// TODO(crbug.com/485932914): Remove [[maybe_unused]] once the implementation is
// complete.
[[maybe_unused]] const base::FeatureParam<AutoPipReason>
    kAutoPictureInPictureSurveyReason{&media::kAutoPictureInPictureSurveys,
                                      "autopip_reason", AutoPipReason::kUnknown,
                                      &kAutoPipReasonOptions};

constexpr base::FeatureParam<PromptResult>::Option kPromptResultOptions[] = {
    {PromptResult::kIgnored, "Ignored"},
    {PromptResult::kBlock, "Block"},
    {PromptResult::kAllowOnEveryVisit, "AllowOnEveryVisit"},
    {PromptResult::kAllowOnce, "AllowOnce"},
    {PromptResult::kNotShownAllowedOnEveryVisit, "NotShownAllowedOnEveryVisit"},
    {PromptResult::kNotShownAllowedOnce, "NotShownAllowedOnce"},
    {PromptResult::kNotShownBlocked, "NotShownBlocked"},
    {PromptResult::kNotShownIncognito, "NotShownIncognito"}};

// TODO(crbug.com/485932914): Remove [[maybe_unused]] once the implementation is
// complete.
[[maybe_unused]] const base::FeatureParam<PromptResult>
    kAutoPictureInPictureSurveyResult{&media::kAutoPictureInPictureSurveys,
                                      "prompt_result", PromptResult::kIgnored,
                                      &kPromptResultOptions};

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
  if (!active_window_context_ || !active_window_context_->prompt_result) {
    active_window_context_ = std::nullopt;
    return;
  }

  // TODO(crbug.com/485932914): Calculate PiP window duration and launch the
  // appropriate HaTS survey based on the prompt result and the AutoPiP reason.

  active_window_context_ = std::nullopt;
}

AutoPictureInPictureHatsService::ActiveWindowContext::ActiveWindowContext(
    media::PictureInPictureEventsInfo::AutoPipReason reason,
    const GURL& origin,
    base::TimeTicks start_time)
    : reason(reason), origin(origin), start_time(start_time) {}
