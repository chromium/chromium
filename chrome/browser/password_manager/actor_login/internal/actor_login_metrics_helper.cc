// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/internal/actor_login_metrics_helper.h"

#include "base/metrics/histogram_functions.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace actor_login {

ActorLoginMetricsHelper::ActorLoginMetricsHelper(ukm::SourceId source_id)
    : source_id_(source_id), builder_(source_id) {}

ActorLoginMetricsHelper::~ActorLoginMetricsHelper() {
  RecordUkm();
}

void ActorLoginMetricsHelper::RecordDeduplicationOccurred(
    bool deduplication_occurred) {
  if (deduplication_occurred) {
    builder_.SetDeduplicationOccurred(true);
    base::UmaHistogramBoolean("Actor.Login.DeduplicationOccurred", true);
  }
}

void ActorLoginMetricsHelper::RecordAccountTypesShown(
    ActorLoginAccountTypes types) {
  builder_.SetAccountTypesShown(static_cast<int64_t>(types));
  base::UmaHistogramEnumeration("Actor.Login.AccountTypesShown", types);
}

void ActorLoginMetricsHelper::RecordNumAccountsShown(int count) {
  builder_.SetNumAccountsShown(count);
  base::UmaHistogramExactLinear("Actor.Login.NumAccountsShown", count,
                                /*exclusive_max=*/4);
}

void ActorLoginMetricsHelper::RecordAccountAutoSelected(bool auto_selected) {
  if (auto_selected) {
    builder_.SetAccountAutoSelected(true);
    base::UmaHistogramBoolean("Actor.Login.AccountAutoSelected", true);
  }
}

void ActorLoginMetricsHelper::RecordSelectedAccountType(
    ActorLoginSelectedAccountType type) {
  builder_.SetSelectedAccountType(static_cast<int64_t>(type));
  base::UmaHistogramEnumeration("Actor.Login.SelectedAccountType", type);
}

void ActorLoginMetricsHelper::OnGetCredentialsStarted() {
  get_credentials_start_time_ = base::TimeTicks::Now();
}

void ActorLoginMetricsHelper::OnGetCredentialsCompleted() {
  get_credentials_completed_time_ = base::TimeTicks::Now();
  CHECK(!get_credentials_start_time_.is_null());
  base::TimeDelta duration =
      get_credentials_completed_time_ - get_credentials_start_time_;
  base::UmaHistogramMediumTimes("Actor.Login.GetCredentialsLatency", duration);
  builder_.SetGetCredentialsLatency(duration.InMilliseconds());
}

void ActorLoginMetricsHelper::OnAccountChosen() {
  account_chosen_time_ = base::TimeTicks::Now();
  // `get_credentials_completed_time_` is null if `OnGetCredentialsCompleted()`
  // was never called (e.g. if the metrics helper was created late in the flow,
  // or if credentials were never fetched).
  if (!get_credentials_completed_time_.is_null()) {
    base::TimeDelta duration =
        account_chosen_time_ - get_credentials_completed_time_;
    base::UmaHistogramMediumTimes(
        "Actor.Login.GetCredentialsCompletedToAccountChosen", duration);
    builder_.SetGetCredentialsCompletedToAccountChosen(
        duration.InMilliseconds());
  }
}

void ActorLoginMetricsHelper::RecordFederatedContinuationShown() {
  builder_.SetFederatedContinuationShown(true);
  base::UmaHistogramBoolean("Actor.Login.Federated.ContinuationShown", true);
}

void ActorLoginMetricsHelper::RecordFederatedLoginResult(
    content::webid::FederatedLoginResult result) {
  if (result == content::webid::FederatedLoginResult::kContinuation) {
    return;
  }
  builder_.SetFederatedLoginResult(static_cast<int64_t>(result));
  base::UmaHistogramEnumeration(
      "Actor.Login.Federated.LoginResult",
      static_cast<ActorLoginFederatedLoginResult>(result));
}

void ActorLoginMetricsHelper::RecordFederatedHangingFedCmRequestExists(
    bool exists) {
  if (exists) {
    builder_.SetFederatedHangingFedCmRequestExists(true);
    base::UmaHistogramBoolean("Actor.Login.Federated.HangingFedCmRequestExists",
                              true);
  }
}

void ActorLoginMetricsHelper::RecordUkm() {
  if (ukm_recorded_ || source_id_ == ukm::kInvalidSourceId) {
    return;
  }
  ukm_recorded_ = true;

  builder_.Record(ukm::UkmRecorder::Get());
}

}  // namespace actor_login
