// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ACTOR_ACTOR_KEY_METRICS_RECORDER_H_
#define CHROME_BROWSER_AUTOFILL_ACTOR_ACTOR_KEY_METRICS_RECORDER_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillClient;
class AutofillManager;
class FormStructure;

// A helper class for `ActorFormFillingServiceImpl` to track and record key
// metrics related to actor-based form filling.
// TODO(crbug.com/487534942): Add metrics recording.
class ActorKeyMetricsRecorder : public AutofillManager::Observer {
 public:
  explicit ActorKeyMetricsRecorder(AutofillClient* client);
  ActorKeyMetricsRecorder(const ActorKeyMetricsRecorder&) = delete;
  ActorKeyMetricsRecorder& operator=(const ActorKeyMetricsRecorder&) = delete;
  ~ActorKeyMetricsRecorder() override;

  // Adds `form_id` to the list of forms that are currently being filled by the actor.
  void RecordFormToFill(FormGlobalId form_id);

  // Tracks that suggestions were generated for `products` on a form.
  void OnSuggestionsGenerated(FormGlobalId form_id,
                              const base::flat_set<FillingProduct>& products);

  // AutofillManager::Observer:
  void OnAfterFormsSeen(AutofillManager& manager,
                        base::span<const FormGlobalId> updated_forms,
                        base::span<const FormGlobalId> removed_forms) override;
  void OnAfterFormSubmitted(AutofillManager& manager,
                            const FormData& form) override;
  void OnFillOrPreviewForm(
      AutofillManager& manager,
      FormGlobalId form_id,
      mojom::ActionPersistence action_persistence,
      const base::flat_set<FieldGlobalId>& filled_field_ids,
      const FillingPayload& filling_payload) override;

 private:
  // Records the key metrics for a `form_structure` if they haven't been
  // recorded yet.
  void RecordKeyMetrics(AutofillManager& manager,
                        const FormStructure& form_structure);

  // Tracks the state of a specific filling product (e.g. Address or Credit
  // Card) for the purpose of metrics recording.
  struct ProductState {
    ProductState();
    ProductState(const ProductState&) = delete;
    ProductState& operator=(const ProductState&) = delete;
    ProductState(ProductState&&);
    ProductState& operator=(ProductState&&);
    ~ProductState();

    // Forms that actor obtained suggestions for.
    base::flat_set<FormGlobalId> with_actor_suggestions;

    // Forms for which key metrics have already been recorded. This ensures
    // metrics are only recorded once per form.
    base::flat_set<FormGlobalId> recorded_forms;

    // Fields that have been filled by the actor.
    base::flat_map<FormGlobalId, base::flat_set<FieldGlobalId>>
        actor_filled_fields;
  };

  // Records the "FillingAssistance" metric for a `form`.
  void RecordFillingAssistance(const FormStructure& form,
                               FillingProduct product);
  void RecordFillingCorrectness(const FormStructure& form,
                                const ProductState& state,
                                FillingProduct product);
  void RecordFillingReadiness(const FormStructure& form,
                              const ProductState& state,
                              FillingProduct product);
  void RecordPerfectFillingMetric(const FormStructure& form,
                                  FillingProduct product);

  bool HasFilledFieldOfProduct(const FormStructure& form,
                               FillingProduct product) const;

  // Returns true if the field with `field_id` in `form` was filled by
  // actor. If `product` is provided, restricts the check to that product.
  bool WasFieldFilledByActor(
      const FormStructure& form,
      FieldGlobalId field_id,
      std::optional<FillingProduct> product = std::nullopt) const;

  std::array<ProductState, std::to_underlying(FillingProduct::kMaxValue) + 1>
      states_;

  ScopedAutofillManagersObservation managers_observation_{this};

  // Forms that `ActorFormFillingService` intends to fill.
  base::flat_set<FormGlobalId> forms_to_fill_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ACTOR_ACTOR_KEY_METRICS_RECORDER_H_
