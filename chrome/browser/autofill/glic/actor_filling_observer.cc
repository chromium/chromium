// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/glic/actor_filling_observer.h"

#include <optional>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "chrome/common/chrome_features.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/autofill/core/browser/integrators/glic/actor_form_filling_types.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/common/unique_ids.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace autofill {

ActorFillingObserver::ActorFillingObserver(
    AutofillClient& autofill_client,
    base::span<const FieldGlobalId> field_ids,
    Callback callback)
    : remaining_field_ids_(field_ids.begin(), field_ids.end()),
      callback_(std::move(callback)) {
  autofill_managers_observation_.Observe(
      &autofill_client, ScopedAutofillManagersObservation::
                            InitializationPolicy::kObservePreexistingManagers);
  credit_card_access_managers_observation_.Observe(&autofill_client);
  // If `remaining_field_ids_` is empty, this will stop the observation and
  // execute `callback_`.
  FinalizeIfComplete();
  UpdateTimeout();
}

ActorFillingObserver::~ActorFillingObserver() {
  Reset();
}

// static
base::TimeDelta ActorFillingObserver::GetFillingTimeout() {
  return ::features::kGlicActorAutofillFillingTimeout.Get();
}

// static
base::TimeDelta ActorFillingObserver::GetMaximumTimeout() {
  return ::features::kGlicActorAutofillMaximumTimeout.Get();
}

std::optional<bool> ActorFillingObserver::IsCreditCardFetchOngoing() const {
  if (!credit_card_access_managers_observation_.IsObserving()) {
    return std::nullopt;
  }
  return ongoing_credit_card_fetches_ > 0;
}

void ActorFillingObserver::OnFillOrPreviewForm(
    AutofillManager&,
    FormGlobalId,
    mojom::ActionPersistence action_persistence,
    const base::flat_set<FieldGlobalId>& filled_field_ids,
    const FillingPayload&) {
  switch (action_persistence) {
    case mojom::ActionPersistence::kFill:
      break;
    case mojom::ActionPersistence::kPreview:
      return;
  }
  for (FieldGlobalId field_id : filled_field_ids) {
    remaining_field_ids_.erase(field_id);
  }
  FinalizeIfComplete();
}

void ActorFillingObserver::OnCreditCardFetchStarted(CreditCardAccessManager&,
                                                    const CreditCard&) {
  ++ongoing_credit_card_fetches_;
  UpdateTimeout();
}
void ActorFillingObserver::OnCreditCardFetchSucceeded(CreditCardAccessManager&,
                                                      const CreditCard&) {
  DecreaseOngoingCreditCardFetches();
  UpdateTimeout();
}
void ActorFillingObserver::OnCreditCardFetchFailed(CreditCardAccessManager&,
                                                   const CreditCard*) {
  DecreaseOngoingCreditCardFetches();
  UpdateTimeout();
}

void ActorFillingObserver::DecreaseOngoingCreditCardFetches() {
  // It is extremely unlikely, but theoretically possible that the credit card
  // access manager was already fetching a card when we started our observation.
  // When that happens, we accept that we may not signal correctly that there
  // is now another credit card fetch ongoing.
  if (ongoing_credit_card_fetches_ > 0) {
    --ongoing_credit_card_fetches_;
  }
}

void ActorFillingObserver::FinalizeIfComplete() {
  if (!remaining_field_ids_.empty()) {
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), base::ok()));
  Reset();
}

void ActorFillingObserver::Reset() {
  autofill_managers_observation_.Reset();
  credit_card_access_managers_observation_.Reset();
  ongoing_credit_card_fetches_ = 0;
  if (callback_) {
    // TODO(crbug.com/455788947): Consider introducing a different type of
    // error.
    // TODO(crbug.com/455788947): Consider not sending an error if some
    // fields were filled.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_),
                       base::unexpected(ActorFormFillingError::kNoForm)));
  }
}

void ActorFillingObserver::UpdateTimeout() {
  if (IsCreditCardFetchOngoing().value_or(false)) {
    timeout_timer_.Stop();
    return;
  }
  if (!timeout_timer_.IsRunning()) {
    timeout_timer_.Start(FROM_HERE, GetFillingTimeout(),
                         base::BindRepeating(&ActorFillingObserver::Reset,
                                             base::Unretained(this)));
  }
}

}  // namespace autofill
