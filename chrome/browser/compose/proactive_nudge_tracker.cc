// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/proactive_nudge_tracker.h"

#include "components/compose/core/browser/config.h"

namespace compose {

ProactiveNudgeTracker::ProactiveNudgeTracker(Delegate* delegate)
    : delegate_(delegate) {}

void ProactiveNudgeTracker::StartObserving(content::WebContents* web_contents) {
  autofill_managers_observation_.Observe(
      web_contents, autofill::ScopedAutofillManagersObservation::
                        InitializationPolicy::kObservePreexistingManagers);
}

ProactiveNudgeTracker::~ProactiveNudgeTracker() = default;

bool ProactiveNudgeTracker::ProactiveNudgeRequestedForFormField(
    const autofill::FormFieldData& field_to_track) {
  DVLOG(2) << "ProactiveNudgeTracker: ProactiveNudgeRequestedForFormField";
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
  timer_.Stop();
  state_ = State();

  state_->form = field_to_track.renderer_form_id();
  state_->field = field_to_track.global_id();
  state_->initial_text_value = field_to_track.value();

  auto delay = compose::GetComposeConfig().proactive_nudge_delay;
  if (delay == base::Milliseconds(0)) {
    state_->show_state = ShowState::kShown;
    return true;
  }

  timer_.Start(FROM_HERE, compose::GetComposeConfig().proactive_nudge_delay,
               this, &ProactiveNudgeTracker::ShowTimerElapsed);
  return false;
}
void ProactiveNudgeTracker::FocusChangedInPage() {
  if (!state_) {
    return;
  }
  state_ = std::nullopt;
  timer_.Stop();
}

void ProactiveNudgeTracker::OnAfterFocusOnFormField(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form,
    autofill::FieldGlobalId field) {
  DVLOG(2) << "ProactiveNudgeTracker: OnAfterFocusOnFormField";
  // If this focus is on the current field, we are (presumably) already focused
  // and this is a no-op. Also, if we are not currently in the WAITING state,
  // this is a no-op.
  if (MatchesCurrentField(form, field) || state_ == std::nullopt) {
    return;
  }

  // Now we should transition to the UNINITIALIZED state.
  state_ = std::nullopt;
  timer_.Stop();
}

void ProactiveNudgeTracker::ShowTimerElapsed() {
  // If we are not in the WAITING state, this timer is stale, we should ignore
  // it.
  if (!state_ || state_->show_state != ShowState::kWaiting) {
    return;
  }

  // Transition to the SHOWN state.
  delegate_->ShowProactiveNudge(state_->form, state_->field);
  state_->show_state = ShowState::kCanBeShown;
}

bool ProactiveNudgeTracker::MatchesCurrentField(autofill::FormGlobalId form,
                                                autofill::FieldGlobalId field) {
  if (state_ == std::nullopt) {
    return false;
  }

  return state_->form == form && state_->field == field;
}

}  // namespace compose
