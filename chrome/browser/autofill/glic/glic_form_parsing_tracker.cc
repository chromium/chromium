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

void GlicFormParsingTracker::OnAutofillManagerStateChanged(
    AutofillManager& manager,
    AutofillDriver::LifecycleState previous,
    AutofillDriver::LifecycleState current) {
  if (previous == AutofillDriver::LifecycleState::kActive &&
      current != AutofillDriver::LifecycleState::kActive) {
    autofill::LocalFrameToken local_frame_token =
        manager.driver().GetFrameToken();
    // TODO(crbug.com/479794574): Do not wait for empty forms when notifying
    // `ObservationDelayController`
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
}

}  // namespace autofill
