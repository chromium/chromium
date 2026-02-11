// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/glic/glic_form_parsing_tracker.h"

namespace autofill {

GlicFormParsingTracker::GlicFormParsingTracker(AutofillClient* client) {
  if (client) {
    autofill_manager_observation_.Observe(client);
  }
}

GlicFormParsingTracker::~GlicFormParsingTracker() = default;

void GlicFormParsingTracker::Wait(base::OnceClosure callback) {
  callbacks_.push_back(std::move(callback));

  // It may happen that forms were parsed before the waiting was requested.
  MaybeNotifyGlic();
}

void GlicFormParsingTracker::MaybeNotifyGlic() {
  if (callbacks_.empty()) {
    return;
  }

  // TODO(crbug.com/479794574): Do not wait for empty forms when notifing
  // `ObservationDelayController`
  bool all_forms_parsed =
      std::ranges::all_of(form_parsing_status_, [](const auto& pair) {
        return pair.second.server_parsed_in_actor_mode &&
               pair.second.heuristic_parsed_in_actor_mode;
      });
  if (all_forms_parsed) {
    for (base::OnceClosure& callback : std::exchange(callbacks_, {})) {
      std::move(callback).Run();
    }
  }
}

void GlicFormParsingTracker::OnAutofillManagerStateChanged(
    AutofillManager& manager,
    AutofillDriver::LifecycleState previous,
    AutofillDriver::LifecycleState current) {
  if (previous == AutofillDriver::LifecycleState::kActive &&
      current != AutofillDriver::LifecycleState::kActive) {
    autofill::LocalFrameToken local_frame_token =
        manager.driver().GetFrameToken();
    absl::erase_if(form_parsing_status_, [local_frame_token](const auto& pair) {
      return pair.first.frame_token == local_frame_token;
    });
  }
}

void GlicFormParsingTracker::OnBeforeFormsSeen(
    AutofillManager& manager,
    base::span<const FormGlobalId> updated_forms,
    base::span<const FormGlobalId> removed_forms) {
  // Insert new forms or invalidate the parsing state of modified forms.
  for (FormGlobalId form_global_id : updated_forms) {
    form_parsing_status_[form_global_id] = FormParsingStatus();
  }
  for (const FormGlobalId& form_global_id : removed_forms) {
    form_parsing_status_.erase(form_global_id);
  }

  MaybeNotifyGlic();
}

void GlicFormParsingTracker::OnFieldTypesDetermined(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form_id,
    FieldTypeSource source,
    bool small_forms_were_parsed) {
  if (!small_forms_were_parsed) {
    return;
  }
  // Parsing might finish after the form got removed from the DOM.
  if (!form_parsing_status_.contains(form_id)) {
    return;
  }

  switch (source) {
    case FieldTypeSource::kHeuristicsOrAutocomplete:
      form_parsing_status_[form_id].heuristic_parsed_in_actor_mode = true;
      break;
    case FieldTypeSource::kAutofillServer:
      form_parsing_status_[form_id].server_parsed_in_actor_mode = true;
      break;
    case FieldTypeSource::kAutofillAiModel:
      // Not supported by GLIC.
      break;
  }

  MaybeNotifyGlic();
}

}  // namespace autofill
