// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_GLIC_ACTOR_FILLING_OBSERVER_H_
#define CHROME_BROWSER_AUTOFILL_GLIC_ACTOR_FILLING_OBSERVER_H_

#include <optional>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/autofill/core/browser/foundations/scoped_credit_card_access_managers_observation.h"
#include "components/autofill/core/browser/integrators/glic/actor_form_filling_types.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/common/unique_ids.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace autofill {

// Helper class for keeping track of completed form fills. It is primarily
// intended to be used inside of `ActorFormFillingServiceImpl`, but exposed
// here for easier testability.
//
// The class observes all `AutofillManager`s associated with `autofill_client`
// and keeps track of the `field_ids` that have been filled. Once all
// `field_ids` have been filled or the `FillingObserver` is destroyed,
// `callback` is called.
// `callback` is always called asynchronously.
class ActorFillingObserver final : public AutofillManager::Observer,
                                   public CreditCardAccessManager::Observer {
 public:
  using Callback =
      base::OnceCallback<void(base::expected<void, ActorFormFillingError>)>;

  ActorFillingObserver(AutofillClient& autofill_client,
                       base::span<const FieldGlobalId> field_ids,
                       Callback callback);
  ~ActorFillingObserver() override;

  // The maximum amount of time to wait for a fill to happen if no credit card
  // fetch is ongoing.
  static base::TimeDelta GetFillingTimeout();

  // The maximum amount of time for which this filling observer should exist.
  static base::TimeDelta GetMaximumTimeout();

 private:
  // AutofillManager::Observer:
  void OnFillOrPreviewForm(
      AutofillManager&,
      FormGlobalId,
      mojom::ActionPersistence action_persistence,
      const base::flat_set<FieldGlobalId>& filled_field_ids,
      const FillingPayload&) override;

  // CreditCardAccessManager::Observer:
  void OnCreditCardFetchStarted(CreditCardAccessManager&,
                                const CreditCard&) override;
  void OnCreditCardFetchSucceeded(CreditCardAccessManager&,
                                  const CreditCard&) override;
  void OnCreditCardFetchFailed(CreditCardAccessManager&,
                               const CreditCard*) override;

  void DecreaseOngoingCreditCardFetches();

  // Calls callback and stops observing `AutofillManager`s if there are no
  // remaining field ids. Otherwise does nothing.
  void FinalizeIfComplete();

  // Returns whether there is least one credit card fetch that was started and
  // that has not yet finished while this observer is active.
  // Returns `std::nullopt` if the observer if all fills have completed and
  // the observer has stopped observing credit card fetching.
  std::optional<bool> IsCreditCardFetchOngoing() const;

  // Stops all observations and runs `callback_` with an error if it is not
  // null.
  void Reset();

  // Updates `timeout_timer_` as follows:
  // - If a credit card fetch is ongoing, stops the timer.
  // - Otherwise starts a timer for resetting `this` (unless one is already
  // running).
  void UpdateTimeout();

  // The fields that have not yet been filled.
  absl::flat_hash_set<FieldGlobalId> remaining_field_ids_;

  // The callback to execute at completion.
  Callback callback_;

  // The observation for the Autofill managers of the relevant tab.
  ScopedAutofillManagersObservation autofill_managers_observation_{this};

  // The observation for the credit card access managers of the relevant tab.
  ScopedCreditCardAccessManagersObservation
      credit_card_access_managers_observation_{this};

  // The number of currently ongoing credit card fetches.
  size_t ongoing_credit_card_fetches_ = 0;

  // A timer that resets `this` when triggered.
  base::OneShotTimer timeout_timer_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_GLIC_ACTOR_FILLING_OBSERVER_H_
