// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/policy_invalidator.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/core/common/cloud/policy_invalidation_util.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {

MetricPolicyRefresh GetPolicyRefreshMetric(bool invalidations_enabled,
                                           bool policy_changed,
                                           bool invalidated) {
  if (policy_changed) {
    if (invalidated) {
      return METRIC_POLICY_REFRESH_INVALIDATED_CHANGED;
    }
    if (invalidations_enabled) {
      return METRIC_POLICY_REFRESH_CHANGED;
    }
    return METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS;
  }
  if (invalidated) {
    return METRIC_POLICY_REFRESH_INVALIDATED_UNCHANGED;
  }
  return METRIC_POLICY_REFRESH_UNCHANGED;
}

PolicyInvalidationType GetInvalidationMetric(bool is_missing_payload,
                                             bool is_expired) {
  if (is_expired) {
    if (is_missing_payload) {
      return POLICY_INVALIDATION_TYPE_NO_PAYLOAD_EXPIRED;
    }
    return POLICY_INVALIDATION_TYPE_EXPIRED;
  }
  if (is_missing_payload) {
    return POLICY_INVALIDATION_TYPE_NO_PAYLOAD;
  }
  return POLICY_INVALIDATION_TYPE_NORMAL;
}

void RecordPolicyRefreshMetric(const char* metric_name,
                               bool invalidations_enabled,
                               bool policy_changed,
                               bool invalidated) {
  const MetricPolicyRefresh metric_policy_refresh = GetPolicyRefreshMetric(
      invalidations_enabled, policy_changed, invalidated);
  base::UmaHistogramEnumeration(metric_name, metric_policy_refresh,
                                METRIC_POLICY_REFRESH_SIZE);
}

void RecordPolicyInvalidationMetric(const char* metric_name,
                                    bool is_expired,
                                    bool is_missing_payload) {
  const PolicyInvalidationType policy_invalidation_type =
      GetInvalidationMetric(is_missing_payload, is_expired);
  base::UmaHistogramEnumeration(metric_name, policy_invalidation_type,
                                POLICY_INVALIDATION_TYPE_SIZE);
}

size_t CalculatePolicyHash(const enterprise_management::PolicyData* policy) {
  if (!policy || !policy->has_policy_value()) {
    return 0;
  }

  return base::FastHash(policy->policy_value());
}

}  // namespace

PolicyInvalidator::PolicyInvalidator(
    PolicyInvalidationScope scope,
    invalidation::InvalidationListener* invalidation_listener,
    CloudPolicyCore* core,
    const base::Clock* clock,
    const std::string& device_local_account_id,
    std::unique_ptr<PolicyInvalidationHandler> policy_invalidation_handler)
    : policy_invalidation_handler_(std::move(policy_invalidation_handler)),
      scope_(scope),
      core_(core),
      invalidation_listener_(invalidation_listener),
      device_local_account_id_(device_local_account_id) {
  CHECK(core_);
  CHECK(invalidation_listener_);
  CHECK(policy_invalidation_handler_);
}

void PolicyInvalidator::Shutdown() {
  // Explicitly reset observation of `InvalidationListener` as it needs
  // `GetType()` to remove observer and `GetType()` requires access to our
  // state.
  invalidation_listener_observation_.Reset();
}

PolicyInvalidator::~PolicyInvalidator() {
  CHECK(!invalidation_listener_observation_.IsObserving());
}

void PolicyInvalidator::OnCoreConnected(CloudPolicyCore* core) {}

void PolicyInvalidator::OnRefreshSchedulerStarted(CloudPolicyCore* core) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(!store_observation_.IsObserving());
  CHECK(!invalidation_listener_observation_.IsObserving());

  auto* cloud_policy_store = core->store();
  CHECK(cloud_policy_store);
  // Policy stack is connected and is ready for invalidations.
  store_observation_.Observe(cloud_policy_store);
  invalidation_listener_observation_.Observe(invalidation_listener_);
  policy_invalidation_handler_->UpdateInvalidationsEnabled(
      AreInvalidationsEnabled());

  // Update the handler with the latest store data.
  policy_invalidation_handler_->HandlePolicyRefresh(cloud_policy_store);
  policy_invalidation_handler_->UpdateMaxFetchDelay(
      cloud_policy_store->policy_map());
}

void PolicyInvalidator::OnCoreDisconnecting(CloudPolicyCore* core) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Policy stack got disconnected and cannot handle policy refresh.
  store_observation_.Reset();
  invalidation_listener_observation_.Reset();
  policy_invalidation_handler_->UpdateInvalidationsEnabled(
      AreInvalidationsEnabled());
}

void PolicyInvalidator::OnStoreLoaded(CloudPolicyStore* store) {
  CHECK_EQ(store, policy_invalidation_handler_->core()->store());

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Policy refreshed due to invalidation or schedule.
  policy_invalidation_handler_->HandlePolicyRefresh(store);
  policy_invalidation_handler_->UpdateMaxFetchDelay(store->policy_map());
}

void PolicyInvalidator::OnStoreError(CloudPolicyStore* store) {}

PolicyInvalidator::PolicyInvalidationHandler::PolicyInvalidationHandler(
    PolicyInvalidationScope scope,
    CloudPolicyCore* core,
    const base::Clock* clock,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : scope_(scope), core_(core), clock_(clock), task_runner_(task_runner) {
  CHECK(task_runner.get());
  // |highest_handled_invalidation_version_| indicates the highest actual
  // invalidation version handled. Since actual invalidations can have only
  // positive versions, this member may be zero (no versioned invalidation
  // handled yet) or positive. Negative values are not allowed:
  //
  // Negative version numbers are used internally by PolicyInvalidator
  // to keep track of unversioned invalidations. When such an invalidation
  // is handled, |highest_handled_invalidation_version_| remains unchanged
  // and does not become negative.
  CHECK_LE(0, highest_handled_invalidation_version_);
}

PolicyInvalidator::PolicyInvalidationHandler::~PolicyInvalidationHandler() =
    default;

void PolicyInvalidator::PolicyInvalidationHandler::HandlePolicyRefresh(
    CloudPolicyStore* store) {
  CHECK(store == core_->store());
  const auto new_policy_hash = CalculatePolicyHash(store->policy());

  if (!policy_hash_value_.has_value()) {
    // On initial policy refresh only store the policy hash.
    policy_hash_value_ = new_policy_hash;
    return;
  }

  const bool policy_changed = policy_hash_value_ != new_policy_hash;
  policy_hash_value_ = new_policy_hash;

  const int64_t store_invalidation_version = store->invalidation_version();
  // Whether the refresh was caused by invalidation.
  const bool invalidated =
      in_progress_invalidation_.has_value() &&
      store_invalidation_version == in_progress_invalidation_.value().version();

  RecordPolicyRefreshMetric(
      GetPolicyRefreshMetricName(scope_),
      /*invalidations_enabled=*/AreInvalidationsEnabledForAWhile(),
      /*policy_changed=*/policy_changed,
      /*invalidated=*/invalidated);

  // If the policy was invalid and the version stored matches the latest
  // invalidation version, that means we handled this exact invalidation.
  if (invalidated) {
    FinishInvalidationHandling();
  }

  // Update the highest invalidation version that was handled already.
  if (store_invalidation_version > highest_handled_invalidation_version_) {
    highest_handled_invalidation_version_ = store_invalidation_version;
  }
}

void PolicyInvalidator::OnExpectationChanged(
    invalidation::InvalidationsExpected expected) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  are_invalidations_expected_ = expected;

  policy_invalidation_handler_->UpdateInvalidationsEnabled(
      AreInvalidationsEnabled());
}

void PolicyInvalidator::OnInvalidationReceived(
    const invalidation::DirectInvalidation& invalidation) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(policy_invalidation_handler_->IsCoreReady())
      << "Policy invalidation received when CloudPolicyCore is disconnected.";

  VLOG_POLICY(1, POLICY_FETCHING)
      << "Received incoming invalidation: " << invalidation.version();

  policy_invalidation_handler_->HandleInvalidation(invalidation);
}

bool PolicyInvalidator::IsRegistered() const {
  return invalidation_listener_observation_.IsObservingSource(
      invalidation_listener_);
}

bool PolicyInvalidator::AreInvalidationsEnabled() const {
  if (!IsRegistered()) {
    return false;
  }

  return are_invalidations_expected_ ==
         invalidation::InvalidationsExpected::kYes;
}

int64_t PolicyInvalidator::highest_handled_invalidation_version() const {
  return policy_invalidation_handler_->highest_handled_invalidation_version();
}

void PolicyInvalidator::PolicyInvalidationHandler::HandleInvalidation(
    const invalidation::DirectInvalidation& invalidation) {
  // Ignore old invalidations.
  if (in_progress_invalidation_.has_value() &&
      invalidation.version() <= in_progress_invalidation_.value().version()) {
    return;
  }

  if (invalidation.version() <= highest_handled_invalidation_version_) {
    // If this invalidation version was handled already, ignore it.
    return;
  }

  // If there is still a pending invalidation, finish it, since we only
  // care about the latest invalidation.
  if (in_progress_invalidation_.has_value()) {
    FinishInvalidationHandling();
  }

  // Ignore the invalidation if it is expired.
  const auto* policy = core_->store()->policy();
  const auto last_fetch_time =
      policy ? base::Time::FromMillisecondsSinceUnixEpoch(policy->timestamp())
             : base::Time();
  const auto current_time = clock_->Now();
  const bool is_expired =
      IsInvalidationExpired(invalidation, last_fetch_time, current_time);
  const bool is_missing_payload = invalidation.payload().empty();

  RecordPolicyInvalidationMetric(GetPolicyInvalidationMetricName(scope_),
                                 is_expired, is_missing_payload);

  if (is_expired) {
    return;
  }

  // Update invalidation state.
  in_progress_invalidation_ = invalidation;

  // In order to prevent the cloud policy server from becoming overwhelmed when
  // a policy with many users is modified, delay for a random period of time
  // before fetching the policy. Delay for at least 20ms so that if multiple
  // invalidations are received in quick succession, only one fetch will be
  // performed.
  base::TimeDelta delay = base::RandTimeDelta(kMinFetchDelay, max_fetch_delay_);

  // Schedule the policy to be refreshed.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &PolicyInvalidator::PolicyInvalidationHandler::RefreshPolicy,
          weak_factory_.GetWeakPtr()),
      delay);
}

void PolicyInvalidator::PolicyInvalidationHandler::UpdateMaxFetchDelay(
    const PolicyMap& policy_map) {
#if !BUILDFLAG(IS_ANDROID)
  // Try reading the delay from the policy.
  const base::Value* delay_policy_value = policy_map.GetValue(
      key::kMaxInvalidationFetchDelay, base::Value::Type::INTEGER);
  if (delay_policy_value) {
    set_max_fetch_delay(base::Milliseconds(delay_policy_value->GetInt()));
    return;
  }
#endif

  set_max_fetch_delay(kMaxFetchDelayDefault);
}

void PolicyInvalidator::PolicyInvalidationHandler::set_max_fetch_delay(
    base::TimeDelta delay) {
  if (delay < kMaxFetchDelayMin) {
    max_fetch_delay_ = kMaxFetchDelayMin;
  } else if (delay > kMaxFetchDelayMax) {
    max_fetch_delay_ = kMaxFetchDelayMax;
  } else {
    max_fetch_delay_ = delay;
  }
}

void PolicyInvalidator::PolicyInvalidationHandler::UpdateInvalidationsEnabled(
    bool invalidations_enabled) {
  if (invalidations_enabled && !invalidations_enabled_time_.has_value()) {
    invalidations_enabled_time_ = clock_->Now();
  } else if (!invalidations_enabled) {
    invalidations_enabled_time_.reset();
  }

  // Refresh scheduler is not available when `core_` is disconnected.
  if (core_->refresh_scheduler()) {
    core_->refresh_scheduler()->SetInvalidationServiceAvailability(
        invalidations_enabled);
  }
}

void PolicyInvalidator::PolicyInvalidationHandler::RefreshPolicy() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(in_progress_invalidation_.has_value())
      << "Policy refresh is scheduled by invalidator without invalidaiton";
  // Set the handled invalidation's info for the upcoming policy fetch.
  core_->client()->SetInvalidationInfo(
      in_progress_invalidation_.value().version(),
      in_progress_invalidation_.value().payload());
  core_->refresh_scheduler()->RefreshSoon(PolicyFetchReason::kInvalidation);
}

bool PolicyInvalidator::PolicyInvalidationHandler::
    AreInvalidationsEnabledForAWhile() const {
  if (!invalidations_enabled_time_.has_value()) {
    return false;
  }

  // Determine if invalidations have been enabled longer than the grace
  // period.
  const base::TimeDelta elapsed =
      clock_->Now() - invalidations_enabled_time_.value();
  return elapsed >= kInvalidationGracePeriod;
}

void PolicyInvalidator::PolicyInvalidationHandler::
    FinishInvalidationHandling() {
  in_progress_invalidation_.reset();
  // Reset client's invalidation info.
  core_->client()->SetInvalidationInfo(/*version=*/0,
                                       /*payload=*/std::string());
  // Cancel any scheduled policy refreshes.
  weak_factory_.InvalidateWeakPtrs();
}

}  // namespace policy
