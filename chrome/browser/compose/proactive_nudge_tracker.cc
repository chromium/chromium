// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/proactive_nudge_tracker.h"

#include "components/compose/core/browser/config.h"
#include "components/segmentation_platform/public/constants.h"

namespace compose {

ProactiveNudgeTracker::State::State() = default;
ProactiveNudgeTracker::State::~State() = default;

ProactiveNudgeTracker::ProactiveNudgeTracker(
    segmentation_platform::SegmentationPlatformService* segmentation_service,
    Delegate* delegate)
    : segmentation_service_(segmentation_service), delegate_(delegate) {}

void ProactiveNudgeTracker::StartObserving(content::WebContents* web_contents) {
  if (!SegmentationStateIsValid()) {
    // Unable to show proactive nudge if configuration is not consistent.
    return;
  }
  autofill_managers_observation_.Observe(
      web_contents, autofill::ScopedAutofillManagersObservation::
                        InitializationPolicy::kObservePreexistingManagers);
}

ProactiveNudgeTracker::~ProactiveNudgeTracker() = default;

bool ProactiveNudgeTracker::ProactiveNudgeRequestedForFormField(
    const autofill::FormFieldData& field_to_track) {
  DVLOG(2) << "ProactiveNudgeTracker: ProactiveNudgeRequestedForFormField";
  if (!SegmentationStateIsValid()) {
    // Unable to show proactive nudge if configuration is not consistent.
    return false;
  }
  if (MatchesCurrentField(field_to_track.renderer_form_id(),
                          field_to_track.global_id())) {
    DVLOG(2) << "ProactiveNudgeTracker: Init with matching field";
    if (state_->show_state == ShowState::kCanBeShown) {
      state_->show_state = ShowState::kShown;
      return true;
    }
    return false;
  }

  // Reset to UNINITIALIZED, then immediately transition to WAITING.
  ResetState();
  state_ = std::make_unique<State>();

  state_->form = field_to_track.renderer_form_id();
  state_->field = field_to_track.global_id();
  state_->initial_text_value = field_to_track.value();

  if (compose::GetComposeConfig().proactive_nudge_segmentation) {
    segmentation_platform::PredictionOptions options;
    options.on_demand_execution = true;
    segmentation_service_->GetClassificationResult(
        segmentation_platform::kComposePromotionKey, options, nullptr,
        base::BindOnce(&ProactiveNudgeTracker::GotClassificationResult,
                       weak_ptr_factory_.GetWeakPtr(), state_->AsWeakPtr()));
  } else {
    state_->segmentation_result = true;
  }

  base::TimeDelta delay = compose::GetComposeConfig().proactive_nudge_delay;
  if (delay == base::Milliseconds(0)) {
    state_->timer_complete = true;
  } else {
    state_->timer.Start(FROM_HERE,
                        compose::GetComposeConfig().proactive_nudge_delay, this,
                        &ProactiveNudgeTracker::ShowTimerElapsed);
  }

  if (state_->segmentation_result && state_->timer_complete) {
    // If the timer is 0-duration and no segmentation result is required, then
    // just transition to Shown state directly before returning true.
    state_->show_state = ShowState::kShown;
    return true;
  }
  return false;
}

void ProactiveNudgeTracker::FocusChangedInPage() {
  ResetState();
}

void ProactiveNudgeTracker::OnAfterFocusOnFormField(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form,
    autofill::FieldGlobalId field) {
  DVLOG(2) << "ProactiveNudgeTracker: OnAfterFocusOnFormField";
  // If this focus is on the current field, we are (presumably) already focused
  // and this is a no-op. Also, if we are not currently in the WAITING state,
  // this is a no-op.
  if (MatchesCurrentField(form, field) || state_ == nullptr) {
    return;
  }

  // Now we should transition to the UNINITIALIZED state.
  ResetState();
}

bool ProactiveNudgeTracker::SegmentationStateIsValid() {
  return !compose::GetComposeConfig().proactive_nudge_segmentation ||
         segmentation_service_ != nullptr;
}

void ProactiveNudgeTracker::ResetState() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  state_.reset();
}

void ProactiveNudgeTracker::ShowTimerElapsed() {
  DVLOG(2) << "ProactiveNudgeTracker: ShowTimerElapsed";
  // If we are not in the WAITING state, this timer is stale, we should ignore
  // it.
  if (!state_ || state_->show_state != ShowState::kWaiting) {
    return;
  }

  state_->timer_complete = true;
  MaybeShowProactiveNudge();
}

void ProactiveNudgeTracker::MaybeShowProactiveNudge() {
  DVLOG(2) << "ProactiveNudgeTracker: MaybeShowProactiveNudge ";
  if (state_ && state_->segmentation_result.value_or(false) &&
      state_->timer_complete) {
    // Transition to the SHOWN state.
    delegate_->ShowProactiveNudge(state_->form, state_->field);
    state_->show_state = ShowState::kCanBeShown;
  }
}

void ProactiveNudgeTracker::GotClassificationResult(
    base::WeakPtr<State> state,
    const segmentation_platform::ClassificationResult& result) {
  if (!state || state->show_state != ShowState::kWaiting) {
    return;
  }

  if (result.status != segmentation_platform::PredictionStatus::kSucceeded) {
    // Do not want to continue with proactive nudge if the segmentation platform
    // had a failure.
    ResetState();
    return;
  }
  state->segmentation_result = result.ordered_labels[0] ==
                               segmentation_platform::kComposePrmotionLabelShow;
  MaybeShowProactiveNudge();
}

bool ProactiveNudgeTracker::MatchesCurrentField(autofill::FormGlobalId form,
                                                autofill::FieldGlobalId field) {
  return state_ && state_->form == form && state_->field == field;
}

}  // namespace compose
