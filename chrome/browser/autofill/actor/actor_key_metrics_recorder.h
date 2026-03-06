// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ACTOR_ACTOR_KEY_METRICS_RECORDER_H_
#define CHROME_BROWSER_AUTOFILL_ACTOR_ACTOR_KEY_METRICS_RECORDER_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillManager;
class FormStructure;

// A helper class for `ActorFormFillingServiceImpl` to track and record key
// metrics related to actor-based form filling.
// TODO(crbug.com/487534942): Add metrics recording.
class ActorKeyMetricsRecorder {
 public:
  ActorKeyMetricsRecorder();
  ActorKeyMetricsRecorder(const ActorKeyMetricsRecorder&) = delete;
  ActorKeyMetricsRecorder& operator=(const ActorKeyMetricsRecorder&) = delete;
  ~ActorKeyMetricsRecorder();

  // Tracks that suggestions were generated for `products` on a form.
  void OnSuggestionsGenerated(FormGlobalId form_id,
                              const base::flat_set<FillingProduct>& products);

  // Tracks that the actor filled a form for `products`. `field_ids` are the
  // fields that were filled.
  void OnFormFilled(FormGlobalId form_id,
                    base::span<const FieldGlobalId> field_ids,
                    const base::flat_set<FillingProduct>& products);

  // Cleans up tracking data for the given forms.
  void OnFormsRemoved(base::span<const FormGlobalId> form_ids);

  // Records the key metrics for a `form_structure` if they haven't been
  // recorded yet.
  void RecordKeyMetrics(AutofillManager& manager,
                        const FormStructure& form_structure);

 private:
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

  // Records the "FillingAssistance" metric for a `form_structure`.
  void RecordFillingAssistance(const FormStructure& form_structure,
                               const ProductState& state,
                               std::string_view product_str);
  void RecordFillingCorrectness(const FormStructure& form_structure,
                                const ProductState& state,
                                std::string_view product_str);
  void RecordFillingReadiness(const FormStructure& form_structure,
                              const ProductState& state,
                              std::string_view product_str);
  void RecordPerfectFillingMetric(const FormStructure& form_structure,
                                  const ProductState& state,
                                  std::string_view product_str);

  std::array<ProductState, std::to_underlying(FillingProduct::kMaxValue) + 1>
      states_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ACTOR_ACTOR_KEY_METRICS_RECORDER_H_
