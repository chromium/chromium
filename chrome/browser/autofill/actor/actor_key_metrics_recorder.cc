// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/actor/actor_key_metrics_recorder.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/suggestions/suggestion_util.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

ActorKeyMetricsRecorder::ProductState::ProductState() = default;
ActorKeyMetricsRecorder::ProductState::ProductState(ProductState&&) = default;
ActorKeyMetricsRecorder::ProductState&
ActorKeyMetricsRecorder::ProductState::operator=(ProductState&&) = default;
ActorKeyMetricsRecorder::ProductState::~ProductState() = default;

ActorKeyMetricsRecorder::ActorKeyMetricsRecorder() = default;
ActorKeyMetricsRecorder::~ActorKeyMetricsRecorder() = default;

void ActorKeyMetricsRecorder::OnSuggestionsGenerated(FormGlobalId form_id,
                                                     FillingProduct product) {
  states_[std::to_underlying(product)].with_actor_suggestions.insert(form_id);
}

void ActorKeyMetricsRecorder::OnFormFilled(
    FormGlobalId form_id,
    base::span<const FieldGlobalId> field_ids,
    FillingProduct product) {
  states_[std::to_underlying(product)].actor_filled_fields[form_id].insert(
      field_ids.begin(), field_ids.end());
}

void ActorKeyMetricsRecorder::OnFormsRemoved(
    base::span<const FormGlobalId> form_ids) {
  for (const FormGlobalId& form_id : form_ids) {
    for (ProductState& state : states_) {
      state.recorded_forms.erase(form_id);
      state.with_actor_suggestions.erase(form_id);
      state.actor_filled_fields.erase(form_id);
    }
  }
}

void ActorKeyMetricsRecorder::RecordKeyMetrics(
    AutofillManager& manager,
    const FormStructure& form_structure) {
  // TODO(crbug.com/487534942): Add key metrics recording.
}

}  // namespace autofill
