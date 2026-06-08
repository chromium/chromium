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
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/suggestions/suggestion_util.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {
namespace {

bool IsFieldOfProduct(const AutofillField& field, FillingProduct product) {
  auto filling_products = FillingProductSet(
      field.Type().GetGroups(), &GetFillingProductFromFieldTypeGroup);
  return filling_products.contains(product);
}

} // namespace


ActorKeyMetricsRecorder::ProductState::ProductState() = default;
ActorKeyMetricsRecorder::ProductState::ProductState(ProductState&&) = default;
ActorKeyMetricsRecorder::ProductState&
ActorKeyMetricsRecorder::ProductState::operator=(ProductState&&) = default;
ActorKeyMetricsRecorder::ProductState::~ProductState() = default;

ActorKeyMetricsRecorder::ActorKeyMetricsRecorder(AutofillClient* client) {
  if (managers_observation_.autofill_driver_factory() == nullptr) {
    managers_observation_.Observe(
        client, ScopedAutofillManagersObservation::InitializationPolicy::
                    kObservePreexistingManagers);
  }
}

ActorKeyMetricsRecorder::~ActorKeyMetricsRecorder() = default;

void ActorKeyMetricsRecorder::RecordFormToFill(FormGlobalId form_id) {
  forms_to_fill_.insert(form_id);
}

void ActorKeyMetricsRecorder::OnSuggestionsGenerated(
    FormGlobalId form_id,
    const base::flat_set<FillingProduct>& products) {
  for (FillingProduct product : products) {
    states_[std::to_underlying(product)].with_actor_suggestions.insert(form_id);
  }
}

void ActorKeyMetricsRecorder::OnAfterFormsSeen(
    AutofillManager& manager,
    base::span<const FormGlobalId> updated_forms,
    base::span<const FormGlobalId> removed_forms) {
  for (const FormGlobalId& form_id : removed_forms) {
    forms_to_fill_.erase(form_id);
    for (ProductState& state : states_) {
      state.recorded_forms.erase(form_id);
      state.with_actor_suggestions.erase(form_id);
      state.actor_filled_fields.erase(form_id);
    }
  }
}

void ActorKeyMetricsRecorder::OnAfterFormSubmitted(AutofillManager& manager,
                                                   const FormData& form) {
  if (const FormStructure* form_structure =
          manager.FindCachedFormById(form.global_id())) {
    RecordKeyMetrics(manager, *form_structure);
  }
  forms_to_fill_.erase(form.global_id());
}

void ActorKeyMetricsRecorder::OnFillOrPreviewForm(
    AutofillManager& manager,
    FormGlobalId form_id,
    FieldGlobalId trigger_field_id,
    mojom::ActionPersistence action_persistence,
    const base::flat_set<FieldGlobalId>& filled_field_ids,
    const FillingPayload& filling_payload) {
  if (!forms_to_fill_.contains(form_id)) {
    return;
  }
  FillingProduct product =
      std::holds_alternative<const CreditCard*>(filling_payload)
          ? FillingProduct::kCreditCard
          : FillingProduct::kAddress;
  states_[std::to_underlying(product)].actor_filled_fields[form_id].insert(
      filled_field_ids.begin(), filled_field_ids.end());
}

void ActorKeyMetricsRecorder::RecordKeyMetrics(AutofillManager& manager,
                                               const FormStructure& form) {
  DenseSet<FormType> form_types =
      form.GetFormTypes(GetAcUnrecognizedBehavior(manager.client()));

  auto record_product_metrics = [&](FillingProduct product, bool is_fillable) {
    if (product != FillingProduct::kAddress &&
        product != FillingProduct::kCreditCard) {
      return;
    }
    ProductState& state = states_[std::to_underlying(product)];
    if (state.recorded_forms.contains(form.global_id())) {
      return;
    }
    state.recorded_forms.insert(form.global_id());

    RecordFillingReadiness(form, state, product);
    RecordPerfectFillingMetric(form, product);
    if (is_fillable) {
      RecordFillingAssistance(form, product);
      RecordFillingCorrectness(form, state, product);
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

  RecordEditedAutofilledFieldAtSubmission(form);
}

void ActorKeyMetricsRecorder::RecordFillingAssistance(
    const FormStructure& form,
    FillingProduct product) {
  base::UmaHistogramBoolean(
      base::StrCat({"Autofill.Actor.KeyMetrics.FillingAssistance.",
                    FillingProductToString(product)}),
      HasFilledFieldOfProduct(form, product));
}

void ActorKeyMetricsRecorder::RecordFillingReadiness(
    const FormStructure& form,
    const ProductState& state,
    FillingProduct product) {
  base::UmaHistogramBoolean(
      base::StrCat({"Autofill.Actor.KeyMetrics.FillingReadiness.",
                    FillingProductToString(product)}),
      state.with_actor_suggestions.contains(form.global_id()));
}

void ActorKeyMetricsRecorder::RecordFillingCorrectness(
    const FormStructure& form,
    const ProductState& state,
    FillingProduct product) {
  if (!HasFilledFieldOfProduct(form, product)) {
    return;
  }

  bool all_unchanged = true;
  for (const std::unique_ptr<AutofillField>& field : form) {
    if (!WasFieldFilledByActor(form, field->global_id(), product)) {
      continue;
    }
    if (field->last_modifier() != FieldModifier::kAutofill) {
      all_unchanged = false;
      break;
    }
  }
  base::UmaHistogramBoolean(
      base::StrCat({"Autofill.Actor.KeyMetrics.FillingCorrectness.",
                    FillingProductToString(product)}),
      all_unchanged);
}

void ActorKeyMetricsRecorder::RecordPerfectFillingMetric(
    const FormStructure& form,
    FillingProduct product) {
  if (!HasFilledFieldOfProduct(form, product)) {
    return;
  }

  bool perfect_filling = std::ranges::none_of(
      form.fields(),
      [this, &form](const std::unique_ptr<AutofillField>& field) {
        // This is a close approximation, since theoretically a user could edit
        // then autofill a field that was filled by the actor, but that should
        // be a very rare case.
        if (WasFieldFilledByActor(form, field->global_id())) {
          return field->all_modifiers().contains(FieldModifier::kUser) &&
                 field->last_modifier() != FieldModifier::kAutofill;
        }

        // If no actor filled it, any modifier means the user intervened
        // (either by typing manually, or by manually triggering standard
        // Autofill).
        return !field->all_modifiers().empty();
      });

  base::UmaHistogramBoolean(
      base::StrCat(
          {"Autofill.Actor.PerfectFilling.", FillingProductToString(product)}),
      perfect_filling);
}

void ActorKeyMetricsRecorder::RecordEditedAutofilledFieldAtSubmission(
    const FormStructure& form) {
  for (const std::unique_ptr<AutofillField>& field : form) {
    if (!WasFieldFilledByActor(form, field->global_id())) {
      continue;
    }
    AutofillMetrics::AutofilledFieldUserEditingStatusMetric editing_metric =
        field->last_modifier() == FieldModifier::kAutofill
            ? AutofillMetrics::AutofilledFieldUserEditingStatusMetric::
                  AUTOFILLED_FIELD_WAS_NOT_EDITED
            : AutofillMetrics::AutofilledFieldUserEditingStatusMetric::
                  AUTOFILLED_FIELD_WAS_EDITED;

    base::UmaHistogramEnumeration(
        "Autofill.Actor.EditedAutofilledFieldAtSubmission.Aggregate",
        editing_metric);
  }
}

bool ActorKeyMetricsRecorder::HasFilledFieldOfProduct(
    const FormStructure& form,
    FillingProduct product) const {
  return std::ranges::any_of(
      form, [this, &form, product](const std::unique_ptr<AutofillField>& field) {
        return IsFieldOfProduct(*field, product) &&
               WasFieldFilledByActor(form, field->global_id(), product);
      });
}

bool ActorKeyMetricsRecorder::WasFieldFilledByActor(
    const FormStructure& form,
    FieldGlobalId field_id,
    std::optional<FillingProduct> product) const {
  auto check_state = [&](const ProductState& state) {
    const base::flat_set<FieldGlobalId>* fields =
        base::FindOrNull(state.actor_filled_fields, form.global_id());
    return fields && fields->contains(field_id);
  };

  if (product) {
    return check_state(states_[std::to_underlying(*product)]);
  }

  return std::ranges::any_of(states_, check_state);
}

}  // namespace autofill
