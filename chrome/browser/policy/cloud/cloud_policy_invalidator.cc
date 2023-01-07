// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/policy/cloud/policy_invalidation_util.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {

MetricPolicyRefresh GetPolicyRefreshMetric(bool invalidations_enabled,
                                           bool policy_changed,
                                           bool invalidated) {
  if (policy_changed) {
    if (invalidated)
      return METRIC_POLICY_REFRESH_INVALIDATED_CHANGED;
    if (invalidations_enabled)
      return METRIC_POLICY_REFRESH_CHANGED;
    return METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS;
  }
  if (invalidated)
    return METRIC_POLICY_REFRESH_INVALIDATED_UNCHANGED;
  return METRIC_POLICY_REFRESH_UNCHANGED;
}

PolicyInvalidationType GetInvalidationMetric(bool is_missing_payload,
                                             bool is_expired) {
  if (is_expired) {
    if (is_missing_payload)
      return POLICY_INVALIDATION_TYPE_NO_PAYLOAD_EXPIRED;
    return POLICY_INVALIDATION_TYPE_EXPIRED;
  }
  if (is_missing_payload)
    return POLICY_INVALIDATION_TYPE_NO_PAYLOAD;
  return POLICY_INVALIDATION_TYPE_NORMAL;
}

void RecordPolicyRefreshMetric(PolicyInvalidationScope scope,
                               bool invalidations_enabled,
                               bool policy_changed,
                               bool invalidated) {
  const MetricPolicyRefresh metric_policy_refresh = GetPolicyRefreshMetric(
      invalidations_enabled, policy_changed, invalidated);
  base::UmaHistogramEnumeration(
      CloudPolicyInvalidator::GetPolicyRefreshMetricName(scope),
      metric_policy_refresh, METRIC_POLICY_REFRESH_SIZE);
  base::UmaHistogramEnumeration(
      CloudPolicyInvalidator::GetPolicyRefreshFcmMetricName(scope),
      metric_policy_refresh, METRIC_POLICY_REFRESH_SIZE);
}

void RecordPolicyInvalidationMetric(PolicyInvalidationScope scope,
                                    bool is_expired,
                                    bool is_missing_payload) {
  const PolicyInvalidationType policy_invalidation_type =
      GetInvalidationMetric(is_missing_payload, is_expired);
  base::UmaHistogramEnumeration(
      CloudPolicyInvalidator::GetPolicyInvalidationMetricName(scope),
      policy_invalidation_type, POLICY_INVALIDATION_TYPE_SIZE);
  base::UmaHistogramEnumeration(
      CloudPolicyInvalidator::GetPolicyInvalidationFcmMetricName(scope),
      policy_invalidation_type, POLICY_INVALIDATION_TYPE_SIZE);
}

std::string ComposeOwnerName(PolicyInvalidationScope scope,
                             const std::string& device_local_account_id) {
  switch (scope) {
    case PolicyInvalidationScope::kUser:
      return "CloudPolicy.User";
    case PolicyInvalidationScope::kDevice:
      return "CloudPolicy.Device";
    case PolicyInvalidationScope::kDeviceLocalAccount:
      DCHECK(!device_local_account_id.empty());
      return base::StrCat(
          {"CloudPolicy.DeviceLocalAccount.", device_local_account_id});
    case PolicyInvalidationScope::kCBCM:
      return "CloudPolicy.CBCM";
  }
}

}  // namespace

const int CloudPolicyInvalidator::kMissingPayloadDelay = 5;
const int CloudPolicyInvalidator::kMaxFetchDelayDefault = 10000;
const int CloudPolicyInvalidator::kMaxFetchDelayMin = 1000;
const int CloudPolicyInvalidator::kMaxFetchDelayMax = 300000;
const int CloudPolicyInvalidator::kInvalidationGracePeriod = 10;

// static
const char* CloudPolicyInvalidator::GetPolicyRefreshMetricName(
    PolicyInvalidationScope scope) {
  switch (scope) {
    case PolicyInvalidationScope::kUser:
      return kMetricUserPolicyRefresh;
    case PolicyInvalidationScope::kDevice:
      return kMetricDevicePolicyRefresh;
    case PolicyInvalidationScope::kDeviceLocalAccount:
      return kMetricDeviceLocalAccountPolicyRefresh;
    case PolicyInvalidationScope::kCBCM:
      return kMetricCBCMPolicyRefresh;
  }
}

// static
const char* CloudPolicyInvalidator::GetPolicyRefreshFcmMetricName(
    PolicyInvalidationScope scope) {
  switch (scope) {
    case PolicyInvalidationScope::kUser:
      return kMetricUserPolicyRefreshFcm;
    case PolicyInvalidationScope::kDevice:
      return kMetricDevicePolicyRefreshFcm;
    case PolicyInvalidationScope::kDeviceLocalAccount:
      return kMetricDeviceLocalAccountPolicyRefreshFcm;
    case PolicyInvalidationScope::kCBCM:
      return kMetricCBCMPolicyRefreshFcm;
  }
}

// static
const char* CloudPolicyInvalidator::GetPolicyInvalidationMetricName(
    PolicyInvalidationScope scope) {
  switch (scope) {
    case PolicyInvalidationScope::kUser:
      return kMetricUserPolicyInvalidations;
    case PolicyInvalidationScope::kDevice:
      return kMetricDevicePolicyInvalidations;
    case PolicyInvalidationScope::kDeviceLocalAccount:
      return kMetricDeviceLocalAccountPolicyInvalidations;
    case PolicyInvalidationScope::kCBCM:
      return kMetricCBCMPolicyInvalidations;
  }
}

// static
const char* CloudPolicyInvalidator::GetPolicyInvalidationFcmMetricName(
    PolicyInvalidationScope scope) {
  switch (scope) {
    case PolicyInvalidationScope::kUser:
      return kMetricUserPolicyInvalidationsFcm;
    case PolicyInvalidationScope::kDevice:
      return kMetricDevicePolicyInvalidationsFcm;
    case PolicyInvalidationScope::kDeviceLocalAccount:
      return kMetricDeviceLocalAccountPolicyInvalidationsFcm;
    case PolicyInvalidationScope::kCBCM:
      return kMetricCBCMPolicyInvalidationsFcm;
  }
}

CloudPolicyInvalidator::CloudPolicyInvalidator(
    PolicyInvalidationScope scope,
    CloudPolicyCore* core,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    base::Clock* clock,
    int64_t highest_handled_invalidation_version)
    : CloudPolicyInvalidator(scope,
                             core,
                             task_runner,
                             clock,
                             highest_handled_invalidation_version,
                             /*device_local_account_id=*/"") {}

CloudPolicyInvalidator::CloudPolicyInvalidator(
    PolicyInvalidationScope scope,
    CloudPolicyCore* core,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    base::Clock* clock,
    int64_t highest_handled_invalidation_version,
    const std::string& device_local_account_id)
    : state_(UNINITIALIZED),
      scope_(scope),
      owner_name_(ComposeOwnerName(scope, device_local_account_id)),
      core_(core),
      task_runner_(task_runner),
      clock_(clock),
      invalidation_service_(nullptr),
      invalidations_enabled_(false),
      invalidation_service_enabled_(false),
      is_registered_(false),
      invalid_(false),
      invalidation_version_(0),
      unknown_version_invalidation_count_(0),
      highest_handled_invalidation_version_(
          highest_handled_invalidation_version),
      max_fetch_delay_(kMaxFetchDelayDefault),
      policy_hash_value_(0) {
  DCHECK(core);
  DCHECK(task_runner.get());
  // |highest_handled_invalidation_version_| indicates the highest actual
  // invalidation version handled. Since actual invalidations can have only
  // positive versions, this member may be zero (no versioned invalidation
  // handled yet) or positive. Negative values are not allowed:
  //
  // Negative version numbers are used internally by CloudPolicyInvalidator to
  // keep track of unversioned invalidations. When such an invalidation is
  // handled, |highest_handled_invalidation_version_| remains unchanged and does
  // not become negative.
  DCHECK_LE(0, highest_handled_invalidation_version_);
}

CloudPolicyInvalidator::~CloudPolicyInvalidator() {
  DCHECK(state_ == SHUT_DOWN);
}

void CloudPolicyInvalidator::Initialize(
    invalidation::InvalidationService* invalidation_service) {
  DCHECK(state_ == UNINITIALIZED);
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(invalidation_service);
  invalidation_service_ = invalidation_service;
  state_ = STOPPED;
  core_->AddObserver(this);
  if (core_->refresh_scheduler())
    OnRefreshSchedulerStarted(core_);
}

void CloudPolicyInvalidator::Shutdown() {
  DCHECK(state_ != SHUT_DOWN);
  DCHECK(thread_checker_.CalledOnValidThread());
  if (state_ == STARTED) {
    if (is_registered_)
      invalidation_service_->UnregisterInvalidationHandler(this);
    core_->store()->RemoveObserver(this);
    weak_factory_.InvalidateWeakPtrs();
  }
  if (state_ != UNINITIALIZED)
    core_->RemoveObserver(this);
  state_ = SHUT_DOWN;
}

void CloudPolicyInvalidator::OnInvalidatorStateChange(
    invalidation::InvalidatorState state) {
  DCHECK(state_ == STARTED);
  DCHECK(thread_checker_.CalledOnValidThread());
  invalidation_service_enabled_ = state == invalidation::INVALIDATIONS_ENABLED;
  UpdateInvalidationsEnabled();
}

void CloudPolicyInvalidator::OnIncomingInvalidation(
    const invalidation::TopicInvalidationMap& invalidation_map) {
  DCHECK(state_ == STARTED);
  DCHECK(thread_checker_.CalledOnValidThread());
  const invalidation::SingleTopicInvalidationSet& list =
      invalidation_map.ForTopic(topic_);
  if (list.IsEmpty()) {
    NOTREACHED();
    return;
  }

  // Acknowledge all except the invalidation with the highest version.
  auto it = list.rbegin();
  ++it;
  for ( ; it != list.rend(); ++it) {
    it->Acknowledge();
  }

  // Handle the highest version invalidation.
  HandleInvalidation(list.back());
}

std::string CloudPolicyInvalidator::GetOwnerName() const {
  return owner_name_;
}

bool CloudPolicyInvalidator::IsPublicTopic(
    const invalidation::Topic& topic) const {
  return IsPublicInvalidationTopic(topic);
}

void CloudPolicyInvalidator::OnCoreConnected(CloudPolicyCore* core) {}

void CloudPolicyInvalidator::OnRefreshSchedulerStarted(CloudPolicyCore* core) {
  DCHECK(state_ == STOPPED);
  DCHECK(thread_checker_.CalledOnValidThread());
  state_ = STARTED;
  OnStoreLoaded(core_->store());
  core_->store()->AddObserver(this);
}

void CloudPolicyInvalidator::OnCoreDisconnecting(CloudPolicyCore* core) {
  DCHECK(state_ == STARTED || state_ == STOPPED);
  DCHECK(thread_checker_.CalledOnValidThread());
  if (state_ == STARTED) {
    Unregister();
    core_->store()->RemoveObserver(this);
    state_ = STOPPED;
  }
}

void CloudPolicyInvalidator::OnStoreLoaded(CloudPolicyStore* store) {
  DCHECK(state_ == STARTED);
  DCHECK(thread_checker_.CalledOnValidThread());
  bool policy_changed = IsPolicyChanged(store->policy());

  if (is_registered_) {
    const int64_t store_invalidation_version = store->invalidation_version();
    // Whether the refresh was caused by invalidation.
    const bool invalidated =
        invalid_ && store_invalidation_version == invalidation_version_;

    const bool invalidations_enabled = GetInvalidationsEnabled();
    RecordPolicyRefreshMetric(scope_, invalidations_enabled, policy_changed,
                              invalidated);

    // If the policy was invalid and the version stored matches the latest
    // invalidation version, acknowledge the latest invalidation.
    if (invalidated)
      AcknowledgeInvalidation();

    // Update the highest invalidation version that was handled already.
    if (store_invalidation_version > highest_handled_invalidation_version_)
      highest_handled_invalidation_version_ = store_invalidation_version;
  }

  UpdateSubscription(store->policy());
  UpdateMaxFetchDelay(store->policy_map());
}

void CloudPolicyInvalidator::OnStoreError(CloudPolicyStore* store) {}

void CloudPolicyInvalidator::HandleInvalidation(
    const invalidation::Invalidation& invalidation) {
  // Ignore old invalidations.
  if (invalid_ && !invalidation.is_unknown_version() &&
      invalidation.version() <= invalidation_version_) {
    return;
  }

  if (!invalidation.is_unknown_version() &&
      invalidation.version() <= highest_handled_invalidation_version_) {
    // If this invalidation version was handled already, acknowledge the
    // invalidation but ignore it otherwise.
    invalidation.Acknowledge();
    return;
  }

  // If there is still a pending invalidation, acknowledge it, since we only
  // care about the latest invalidation.
  if (invalid_)
    AcknowledgeInvalidation();

  // Get the version and payload from the invalidation.
  // When an invalidation with unknown version is received, use negative
  // numbers based on the number of such invalidations received. This
  // ensures that the version numbers do not collide with "real" versions
  // (which are positive) or previous invalidations with unknown version.
  int64_t version;
  std::string payload;
  if (invalidation.is_unknown_version()) {
    version = -(++unknown_version_invalidation_count_);
  } else {
    version = invalidation.version();
    payload = invalidation.payload();
  }

  // Ignore the invalidation if it is expired.
  const auto last_fetch_time =
      base::Time::FromJavaTime(core_->store()->policy()->timestamp());
  const auto current_time = clock_->Now();
  const bool is_expired =
      IsInvalidationExpired(invalidation, last_fetch_time, current_time);
  const bool is_missing_payload = payload.empty();

  RecordPolicyInvalidationMetric(scope_, is_expired, is_missing_payload);

  if (is_expired) {
    invalidation.Acknowledge();
    return;
  }

  // Update invalidation state.
  invalid_ = true;
  invalidation_ = std::make_unique<invalidation::Invalidation>(invalidation);
  invalidation_version_ = version;

  // In order to prevent the cloud policy server from becoming overwhelmed when
  // a policy with many users is modified, delay for a random period of time
  // before fetching the policy. Delay for at least 20ms so that if multiple
  // invalidations are received in quick succession, only one fetch will be
  // performed.
  base::TimeDelta delay =
      base::Milliseconds(base::RandInt(20, max_fetch_delay_));

  // If there is a payload, the policy can be refreshed at any time, so set
  // the version and payload on the client immediately. Otherwise, the refresh
  // must only run after at least kMissingPayloadDelay minutes.
  if (!payload.empty())
    core_->client()->SetInvalidationInfo(version, payload);
  else
    delay += base::Minutes(kMissingPayloadDelay);

  // Schedule the policy to be refreshed.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CloudPolicyInvalidator::RefreshPolicy,
                     weak_factory_.GetWeakPtr(),
                     payload.empty() /* is_missing_payload */),
      delay);
}

void CloudPolicyInvalidator::UpdateSubscription(
    const enterprise_management::PolicyData* policy) {
  // Create the Topic based on the policy data.
  // If the policy does not specify a Topic, then unregister.
  invalidation::Topic topic;
  if (!policy || !GetCloudPolicyTopicFromPolicy(*policy, &topic)) {
    Unregister();
    return;
  }

  // If the policy topic in the policy data is different from the currently
  // registered topic, update the object registration.
  if (!is_registered_ || topic != topic_)
    Register(topic);
}

void CloudPolicyInvalidator::Register(const invalidation::Topic& topic) {
  // Register this handler with the invalidation service if needed.
  if (!is_registered_) {
    OnInvalidatorStateChange(invalidation_service_->GetInvalidatorState());
    invalidation_service_->RegisterInvalidationHandler(this);
  }

  // Update internal state.
  if (invalid_)
    AcknowledgeInvalidation();
  is_registered_ = true;
  topic_ = topic;
  UpdateInvalidationsEnabled();

  // Update subscription with the invalidation service.
  bool success =
      invalidation_service_->UpdateInterestedTopics(this, /*topics=*/{topic});
  // Do not crash as server might send duplicate invalidation IDs due to
  // http://b/119860379.
  if (!success) {
    LOG(ERROR) << "Failed to subscribe to " << topic
               << " for policy invalidations";
  }
  base::UmaHistogramBoolean(kMetricPolicyInvalidationRegistration, success);
  base::UmaHistogramBoolean(kMetricPolicyInvalidationRegistrationFcm, success);
}

void CloudPolicyInvalidator::Unregister() {
  if (is_registered_) {
    if (invalid_)
      AcknowledgeInvalidation();
    CHECK(invalidation_service_->UpdateInterestedTopics(
        this, invalidation::TopicSet()));
    invalidation_service_->UnregisterInvalidationHandler(this);
    is_registered_ = false;
    UpdateInvalidationsEnabled();
  }
}

void CloudPolicyInvalidator::UpdateMaxFetchDelay(const PolicyMap& policy_map) {
#if !BUILDFLAG(IS_ANDROID)
  // Try reading the delay from the policy.
  const base::Value* delay_policy_value = policy_map.GetValue(
      key::kMaxInvalidationFetchDelay, base::Value::Type::INTEGER);
  if (delay_policy_value) {
    set_max_fetch_delay(delay_policy_value->GetInt());
    return;
  }
#endif

  set_max_fetch_delay(kMaxFetchDelayDefault);
}

void CloudPolicyInvalidator::set_max_fetch_delay(int delay) {
  if (delay < kMaxFetchDelayMin)
    max_fetch_delay_ = kMaxFetchDelayMin;
  else if (delay > kMaxFetchDelayMax)
    max_fetch_delay_ = kMaxFetchDelayMax;
  else
    max_fetch_delay_ = delay;
}

void CloudPolicyInvalidator::UpdateInvalidationsEnabled() {
  bool invalidations_enabled = invalidation_service_enabled_ && is_registered_;
  if (invalidations_enabled_ != invalidations_enabled) {
    invalidations_enabled_ = invalidations_enabled;
    if (invalidations_enabled)
      invalidations_enabled_time_ = clock_->Now();
    core_->refresh_scheduler()->SetInvalidationServiceAvailability(
        invalidations_enabled);
  }
}

void CloudPolicyInvalidator::RefreshPolicy(bool is_missing_payload) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // In the missing payload case, the invalidation version has not been set on
  // the client yet, so set it now that the required time has elapsed.
  if (is_missing_payload)
    core_->client()->SetInvalidationInfo(invalidation_version_, std::string());
  core_->refresh_scheduler()->RefreshSoon();
}

void CloudPolicyInvalidator::AcknowledgeInvalidation() {
  DCHECK(invalid_);
  invalid_ = false;
  core_->client()->SetInvalidationInfo(0, std::string());
  invalidation_->Acknowledge();
  invalidation_.reset();
  // Cancel any scheduled policy refreshes.
  weak_factory_.InvalidateWeakPtrs();
}

bool CloudPolicyInvalidator::IsPolicyChanged(
    const enterprise_management::PolicyData* policy) {
  // Determine if the policy changed by comparing its hash value to the
  // previous policy's hash value.
  uint32_t new_hash_value = 0;
  if (policy && policy->has_policy_value())
    new_hash_value = base::Hash(policy->policy_value());
  bool changed = new_hash_value != policy_hash_value_;
  policy_hash_value_ = new_hash_value;
  return changed;
}

bool CloudPolicyInvalidator::GetInvalidationsEnabled() {
  if (!invalidations_enabled_)
    return false;
  // If invalidations have been enabled for less than the grace period, then
  // consider invalidations to be disabled for metrics reporting.
  base::TimeDelta elapsed = clock_->Now() - invalidations_enabled_time_;
  return elapsed.InSeconds() >= kInvalidationGracePeriod;
}

}  // namespace policy
