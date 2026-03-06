// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/actor/actor_key_metrics_recorder.h"

#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/containers/map_util.h"
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

void ActorKeyMetricsRecorder::OnSuggestionsGenerated(
    FormGlobalId form_id,
    const base::flat_set<FillingProduct>& products) {
  for (FillingProduct product : products) {
    states_[std::to_underlying(product)].with_actor_suggestions.insert(form_id);
  }
}

void ActorKeyMetricsRecorder::OnFormFilled(
    FormGlobalId form_id,
    base::span<const FieldGlobalId> field_ids,
    const base::flat_set<FillingProduct>& products) {
  for (FillingProduct product : products) {
    states_[std::to_underlying(product)].actor_filled_fields[form_id].insert(
        field_ids.begin(), field_ids.end());
  }
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
  DenseSet<FormType> form_types =
      form_structure.GetFormTypes(GetAcUnrecognizedBehavior(manager.client()));

  auto record_product_metrics = [&](FillingProduct product, bool is_fillable) {
    if (product != FillingProduct::kAddress &&
        product != FillingProduct::kCreditCard) {
      return;
    }
    ProductState& state = states_[std::to_underlying(product)];
    if (state.recorded_forms.contains(form_structure.global_id())) {
      return;
    }
    state.recorded_forms.insert(form_structure.global_id());

    // TODO(crbug.com/487534942): Add more key metrics.
    const std::string product_str = FillingProductToString(product);

    RecordFillingReadiness(form_structure, state, product_str);
    RecordPerfectFillingMetric(form_structure, state, product_str);
    if (is_fillable) {
      RecordFillingAssistance(form_structure, state, product_str);
      RecordFillingCorrectness(form_structure, state, product_str);
    }
  };

  if (form_types.contains(FormType::kAddressForm)) {
    record_product_metrics(FillingProduct::kAddress,
                           !manager.client()
                                .GetPersonalDataManager()
                                .address_data_manager()
                                .GetProfiles()
                                .empty());
  }

  if (form_types.contains(FormType::kCreditCardForm)) {
    record_product_metrics(FillingProduct::kCreditCard,
                           !manager.client()
                                .GetPersonalDataManager()
                                .payments_data_manager()
                                .GetCreditCards()
                                .empty());
  }
}

void ActorKeyMetricsRecorder::RecordFillingAssistance(
    const FormStructure& form_structure,
    const ProductState& state,
    std::string_view product_str) {
  base::UmaHistogramBoolean(
      base::StrCat(
          {"Autofill.Actor.KeyMetrics.FillingAssistance.", product_str}),
      state.actor_filled_fields.find(form_structure.global_id()) !=
          state.actor_filled_fields.end());
}

void ActorKeyMetricsRecorder::RecordFillingReadiness(
    const FormStructure& form_structure,
    const ProductState& state,
    std::string_view product_str) {
  base::UmaHistogramBoolean(
      base::StrCat(
          {"Autofill.Actor.KeyMetrics.FillingReadiness.", product_str}),
      state.with_actor_suggestions.contains(form_structure.global_id()));
}

void ActorKeyMetricsRecorder::RecordFillingCorrectness(
    const FormStructure& form_structure,
    const ProductState& state,
    std::string_view product_str) {
  if (state.actor_filled_fields.find(form_structure.global_id()) ==
      state.actor_filled_fields.end()) {
    return;
  }

  if (const base::flat_set<FieldGlobalId>* filled_fields = base::FindOrNull(
          state.actor_filled_fields, form_structure.global_id())) {
    bool all_unchanged = true;
    bool has_actor_fields = false;
    for (const std::unique_ptr<AutofillField>& field : form_structure) {
      if (!filled_fields->contains(field->global_id())) {
        continue;
      }
      has_actor_fields = true;
      if (field->last_modifier() != FieldModifier::kAutofill) {
        all_unchanged = false;
        break;
      }
    }
    if (has_actor_fields) {
      base::UmaHistogramBoolean(
          base::StrCat(
              {"Autofill.Actor.KeyMetrics.FillingCorrectness.", product_str}),
          all_unchanged);
    }
  }
}

void ActorKeyMetricsRecorder::RecordPerfectFillingMetric(
    const FormStructure& form_structure,
    const ProductState& state,
    std::string_view product_str) {
  if (!state.actor_filled_fields.contains(form_structure.global_id())) {
    return;
  }

  // Checks if a specific field was filled by an actor.
  auto is_filled_by_actor = [&](FieldGlobalId field_id) {
    return std::ranges::any_of(states_, [&form_structure, field_id](
                                            const ProductState& product_state) {
      const base::flat_set<FieldGlobalId>* fields = base::FindOrNull(
          product_state.actor_filled_fields, form_structure.global_id());
      return fields && fields->contains(field_id);
    });
  };

  bool perfect_filling = std::ranges::none_of(
      form_structure.fields(),
      [&is_filled_by_actor](const std::unique_ptr<AutofillField>& field) {
        // This is a close approximation, since theoretically a user could edit
        // then autofill a field that was filled by the actor, but that should
        // be a very rare case.
        if (is_filled_by_actor(field->global_id())) {
          return field->all_modifiers().contains(FieldModifier::kUser) &&
                 field->last_modifier() != FieldModifier::kAutofill;
        }

        // If no actor filled it, any modifier means the user intervened
        // (either by typing manually, or by manually triggering standard
        // Autofill).
        return !field->all_modifiers().empty();
      });

  base::UmaHistogramBoolean(
      base::StrCat({"Autofill.Actor.PerfectFilling.", product_str}),
      perfect_filling);
}

}  // namespace autofill
