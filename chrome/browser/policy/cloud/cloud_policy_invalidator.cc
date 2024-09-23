// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/hash/hash.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/invalidation/invalidation_factory.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/core/common/cloud/policy_invalidation_util.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {

constexpr char kDevicePolicyInvalidatorTypeName[] = "DEVICE_POLICY_FETCH";
constexpr char kBrowserPolicyInvalidatorTypeName[] = "BROWSER_POLICY_FETCH";
constexpr char kUserPolicyInvalidatorTypeName[] = "USER_POLICY_FETCH";
constexpr char kDeviceLocalAccountPolicyInvalidatorTypeNameTemplate[] =
    "PUBLIC_ACCOUNT_POLICY_FETCH-%s";

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
}

void RecordPolicyInvalidationMetric(PolicyInvalidationScope scope,
                                    bool is_expired,
                                    bool is_missing_payload) {
  const PolicyInvalidationType policy_invalidation_type =
      GetInvalidationMetric(is_missing_payload, is_expired);
  base::UmaHistogramEnumeration(
      CloudPolicyInvalidator::GetPolicyInvalidationMetricName(scope),
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
      CHECK(!device_local_account_id.empty());
      return base::StrCat(
          {"CloudPolicy.DeviceLocalAccount.", device_local_account_id});
    case PolicyInvalidationScope::kCBCM:
      return "CloudPolicy.CBCM";
  }
}

auto CalculatePolicyHash(const enterprise_management::PolicyData* policy) {
  if (!policy || !policy->has_policy_value()) {
    return 0u;
  }

  return base::Hash(policy->policy_value());
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

CloudPolicyInvalidator::PolicyInvalidationHandler::PolicyInvalidationHandler(
    PolicyInvalidationScope scope,
    int64_t highest_handled_invalidation_version,
    CloudPolicyCore* core,
    base::Clock* clock,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : scope_(scope),
      core_(core),
      highest_handled_invalidation_version_(
          highest_handled_invalidation_version),
      clock_(clock),
      task_runner_(task_runner) {
  CHECK(task_runner.get());
  // |highest_handled_invalidation_version_| indicates the highest actual
  // invalidation version handled. Since actual invalidations can have only
  // positive versions, this member may be zero (no versioned invalidation
  // handled yet) or positive. Negative values are not allowed:
  //
  // Negative version numbers are used internally by CloudPolicyInvalidator
  // to keep track of unversioned invalidations. When such an invalidation
  // is handled, |highest_handled_invalidation_version_| remains unchanged
  // and does not become negative.
  CHECK_LE(0, highest_handled_invalidation_version_);
}

CloudPolicyInvalidator::PolicyInvalidationHandler::
    ~PolicyInvalidationHandler() = default;

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
    : state_(State::UNINITIALIZED),
      policy_invalidation_handler_(scope,
                                   highest_handled_invalidation_version,
                                   core,
                                   clock,
                                   std::move(task_runner)),
      scope_(scope),
      owner_name_(ComposeOwnerName(scope, device_local_account_id)),
      core_(core),
      device_local_account_id_(device_local_account_id) {
  CHECK(core);
}

CloudPolicyInvalidator::~CloudPolicyInvalidator() {
  CHECK(state_ == State::SHUT_DOWN);
}

void CloudPolicyInvalidator::Initialize(
    std::variant<invalidation::InvalidationService*,
                 invalidation::InvalidationListener*>
        invalidation_service_or_listener) {
  CHECK(state_ == State::UNINITIALIZED);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(!std::holds_alternative<invalidation::InvalidationService*>(
            invalidation_service_or_listener) ||
        std::get<invalidation::InvalidationService*>(
            invalidation_service_or_listener))
      << "InvalidationService is used but is null";
  CHECK(!std::holds_alternative<invalidation::InvalidationListener*>(
            invalidation_service_or_listener) ||
        std::get<invalidation::InvalidationListener*>(
            invalidation_service_or_listener))
      << "InvalidationListener is used but is null";
  invalidation_service_or_listener_ = invalidation::PointerVariantToRawPointer(
      invalidation_service_or_listener);
  state_ = State::STOPPED;
  core_observation_.Observe(core_);
  if (core_->refresh_scheduler())
    OnRefreshSchedulerStarted(core_);
}

void CloudPolicyInvalidator::Shutdown() {
  CHECK(state_ != State::SHUT_DOWN);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  invalidation_service_observation_.Reset();
  invalidation_listener_observation_.Reset();

  if (state_ == State::STARTED) {
    policy_invalidation_handler_.CancelInvalidationHandling();
  }
  core_observation_.Reset();
  std::visit([](auto& v) { v = nullptr; }, invalidation_service_or_listener_);
  state_ = State::SHUT_DOWN;
}

void CloudPolicyInvalidator::OnInvalidatorStateChange(
    invalidation::InvalidatorState state) {
  CHECK(state_ == State::STARTED);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  policy_invalidation_handler_.UpdateInvalidationsEnabled(
      AreInvalidationsEnabled());
}

void CloudPolicyInvalidator::OnIncomingInvalidation(
    const invalidation::Invalidation& invalidation) {
  CHECK(state_ == State::STARTED);
  CHECK(invalidation.topic() == topic_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  policy_invalidation_handler_.HandleInvalidation(invalidation);
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
  CHECK(state_ == State::STOPPED);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  state_ = State::STARTED;
  OnStoreLoaded(core_->store());
  store_observation_.Observe(core_->store());
}

void CloudPolicyInvalidator::OnCoreDisconnecting(CloudPolicyCore* core) {
  CHECK(state_ == State::STARTED || state_ == State::STOPPED);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (state_ == State::STARTED) {
    std::visit(base::Overloaded{
                   [this](invalidation::InvalidationService* service) {
                     UnregisterWithInvalidationService();
                   },
                   [](invalidation::InvalidationListener* listener) {
                     // Do nothing.
                   },
               },
               invalidation_service_or_listener_);
    store_observation_.Reset();
    state_ = State::STOPPED;
  }
}

void CloudPolicyInvalidator::OnStoreLoaded(CloudPolicyStore* store) {
  CHECK(state_ == State::STARTED);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  policy_invalidation_handler_.HandlePolicyRefresh(
      store, /*is_registered_for_invalidations=*/IsRegistered(),
      /*invalidations_enabled=*/AreInvalidationsEnabled());

  std::visit(base::Overloaded{
                 [this, store](invalidation::InvalidationService* service) {
                   UpdateSubscriptionWithInvalidationService(store->policy());
                 },
                 [this](invalidation::InvalidationListener* listener) {
                   HandleOnStoreLoadedForListener(listener);
                 },
             },
             invalidation_service_or_listener_);

  policy_invalidation_handler_.UpdateMaxFetchDelay(store->policy_map());
}

void CloudPolicyInvalidator::PolicyInvalidationHandler::HandlePolicyRefresh(
    CloudPolicyStore* store,
    bool is_registered_for_invalidations,
    bool invalidations_enabled) {
  const auto new_policy_hash = CalculatePolicyHash(store->policy());
  const bool policy_changed = policy_hash_value_ != new_policy_hash;
  policy_hash_value_ = new_policy_hash;

  if (!is_registered_for_invalidations) {
    return;
  }

  const int64_t store_invalidation_version = store->invalidation_version();
  // Whether the refresh was caused by invalidation.
  const bool invalidated =
      invalid_ && store_invalidation_version == invalidation_version_;

  RecordPolicyRefreshMetric(
      scope_,
      /*invalidations_enabled=*/invalidations_enabled &&
          HaveInvalidationsBeenEnabledForAWhileForMetricsRecording(),
      /*policy_changed=*/policy_changed, /*invalidated=*/invalidated);

  // If the policy was invalid and the version stored matches the latest
  // invalidation version, acknowledge the latest invalidation.
  if (invalidated) {
    AcknowledgeInvalidation();
  }

  // Update the highest invalidation version that was handled already.
  if (store_invalidation_version > highest_handled_invalidation_version_) {
    highest_handled_invalidation_version_ = store_invalidation_version;
  }
}

void CloudPolicyInvalidator::OnStoreError(CloudPolicyStore* store) {}

void CloudPolicyInvalidator::OnExpectationChanged(
    invalidation::InvalidationsExpected expected) {
  CHECK(state_ == State::STARTED);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  are_invalidations_expected_ = expected;

  policy_invalidation_handler_.UpdateInvalidationsEnabled(
      AreInvalidationsEnabled());
}

void CloudPolicyInvalidator::OnInvalidationReceived(
    const invalidation::DirectInvalidation& invalidation) {
  CHECK(state_ == State::STARTED);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  LOG(WARNING) << "Received incoming invalidation: " << invalidation.version();
  policy_invalidation_handler_.HandleInvalidation(invalidation);
}

std::string CloudPolicyInvalidator::GetType() const {
  switch (scope_) {
    case PolicyInvalidationScope::kUser:
      return kUserPolicyInvalidatorTypeName;
    case PolicyInvalidationScope::kDevice:
      return kDevicePolicyInvalidatorTypeName;
    case PolicyInvalidationScope::kCBCM:
      return kBrowserPolicyInvalidatorTypeName;
    case PolicyInvalidationScope::kDeviceLocalAccount:
      return base::StringPrintf(
          kDeviceLocalAccountPolicyInvalidatorTypeNameTemplate,
          device_local_account_id_.c_str());
  }
}

bool CloudPolicyInvalidator::IsRegistered() const {
  return std::visit(
      base::Overloaded{
          [this](invalidation::InvalidationService* service) {
            return service &&
                   invalidation_service_observation_.IsObservingSource(service);
          },
          [this](invalidation::InvalidationListener* listener) {
            return listener &&
                   invalidation_listener_observation_.IsObservingSource(
                       listener);
          }},
      invalidation_service_or_listener_);
}

bool CloudPolicyInvalidator::AreInvalidationsEnabled() const {
  if (!IsRegistered()) {
    return false;
  }

  return std::visit(
      base::Overloaded{[](invalidation::InvalidationService* service) {
                         return service->GetInvalidatorState() ==
                                invalidation::InvalidatorState::kEnabled;
                       },
                       [this](invalidation::InvalidationListener* listener) {
                         return are_invalidations_expected_ ==
                                invalidation::InvalidationsExpected::kYes;
                       }},
      invalidation_service_or_listener_);
}

void CloudPolicyInvalidator::PolicyInvalidationHandler::HandleInvalidation(
    const invalidation::Invalidation& invalidation) {
  // Ignore old invalidations.
  if (invalid_ && invalidation.version() <= invalidation_version_) {
    return;
  }

  if (invalidation.version() <= highest_handled_invalidation_version_) {
    // If this invalidation version was handled already, ignore it.
    return;
  }

  // If there is still a pending invalidation, acknowledge it, since we only
  // care about the latest invalidation.
  if (invalid_)
    AcknowledgeInvalidation();

  // Get the version and payload from the invalidation.
  const int64_t version = invalidation.version();
  const std::string payload = invalidation.payload();

  // Ignore the invalidation if it is expired.
  const auto last_fetch_time = base::Time::FromMillisecondsSinceUnixEpoch(
      core_->store()->policy()->timestamp());
  const auto current_time = clock_->Now();
  const bool is_expired =
      IsInvalidationExpired(invalidation, last_fetch_time, current_time);
  const bool is_missing_payload = payload.empty();

  RecordPolicyInvalidationMetric(scope_, is_expired, is_missing_payload);

  if (is_expired) {
    return;
  }

  // Update invalidation state.
  invalid_ = true;
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
      base::BindOnce(
          &CloudPolicyInvalidator::PolicyInvalidationHandler::RefreshPolicy,
          weak_factory_.GetWeakPtr(), payload.empty() /* is_missing_payload */),
      delay);
}

void CloudPolicyInvalidator::UpdateSubscriptionWithInvalidationService(
    const enterprise_management::PolicyData* policy) {
  CHECK(std::holds_alternative<InvalidationServicePtr>(
      invalidation_service_or_listener_));

  // Create the Topic based on the policy data.
  // If the policy does not specify a Topic, then unregister.
  invalidation::Topic topic;
  if (!policy || !GetCloudPolicyTopicFromPolicy(*policy, &topic)) {
    UnregisterWithInvalidationService();
    return;
  }

  // If the policy topic in the policy data is different from the currently
  // registered topic, update the object registration.
  if (!IsRegistered() || topic != topic_) {
    RegisterWithInvalidationService(topic);
  }
}

void CloudPolicyInvalidator::HandleOnStoreLoadedForListener(
    invalidation::InvalidationListener* listener) {
  if (!IsRegistered()) {
    invalidation_listener_observation_.Observe(listener);
  }
  policy_invalidation_handler_.UpdateInvalidationsEnabled(
      AreInvalidationsEnabled());
}

void CloudPolicyInvalidator::RegisterWithInvalidationService(
    const invalidation::Topic& topic) {
  CHECK(std::holds_alternative<InvalidationServicePtr>(
      invalidation_service_or_listener_));
  auto invalidation_service =
      std::get<InvalidationServicePtr>(invalidation_service_or_listener_);

  // Register this handler with the invalidation service if needed.
  if (!IsRegistered()) {
    invalidation_service_observation_.Observe(invalidation_service);
  }

  // Update internal state.
  policy_invalidation_handler_.CancelInvalidationHandlingIfWaitingForOne();
  topic_ = topic;
  policy_invalidation_handler_.UpdateInvalidationsEnabled(
      AreInvalidationsEnabled());

  // Update subscription with the invalidation service.
  const bool success =
      invalidation_service->UpdateInterestedTopics(this, /*topics=*/{topic});
  CHECK(success) << "Could not subscribe to topic: " << topic;
}

void CloudPolicyInvalidator::UnregisterWithInvalidationService() {
  CHECK(std::holds_alternative<InvalidationServicePtr>(
      invalidation_service_or_listener_));
  auto invalidation_service =
      std::get<InvalidationServicePtr>(invalidation_service_or_listener_);

  if (IsRegistered()) {
    policy_invalidation_handler_.CancelInvalidationHandlingIfWaitingForOne();
    CHECK(invalidation_service->UpdateInterestedTopics(
        this, invalidation::TopicSet()));
    invalidation_service_observation_.Reset();
    policy_invalidation_handler_.UpdateInvalidationsEnabled(
        AreInvalidationsEnabled());
  }
}

void CloudPolicyInvalidator::PolicyInvalidationHandler::UpdateMaxFetchDelay(
    const PolicyMap& policy_map) {
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

void CloudPolicyInvalidator::PolicyInvalidationHandler::set_max_fetch_delay(
    int delay) {
  if (delay < kMaxFetchDelayMin)
    max_fetch_delay_ = kMaxFetchDelayMin;
  else if (delay > kMaxFetchDelayMax)
    max_fetch_delay_ = kMaxFetchDelayMax;
  else
    max_fetch_delay_ = delay;
}

void CloudPolicyInvalidator::PolicyInvalidationHandler::
    UpdateInvalidationsEnabled(bool invalidations_enabled) {
  if (invalidations_enabled && !invalidations_enabled_time_.has_value()) {
    invalidations_enabled_time_ = clock_->Now();
  } else if (!invalidations_enabled) {
    invalidations_enabled_time_.reset();
  }

  // Since invalidator never stops observing `InvalidationListener`, refresh
  // scheduler may be unavailable when invalidation stack is still working.
  if (core_->refresh_scheduler()) {
    core_->refresh_scheduler()->SetInvalidationServiceAvailability(
        invalidations_enabled);
  }
}

void CloudPolicyInvalidator::PolicyInvalidationHandler::RefreshPolicy(
    bool is_missing_payload) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // In the missing payload case, the invalidation version has not been set on
  // the client yet, so set it now that the required time has elapsed.
  if (is_missing_payload)
    core_->client()->SetInvalidationInfo(invalidation_version_, std::string());
  core_->refresh_scheduler()->RefreshSoon(PolicyFetchReason::kInvalidation);
}

void CloudPolicyInvalidator::PolicyInvalidationHandler::
    AcknowledgeInvalidation() {
  CHECK(invalid_);
  invalid_ = false;
  core_->client()->SetInvalidationInfo(0, std::string());
  // Cancel any scheduled policy refreshes.
  CancelInvalidationHandling();
}

bool CloudPolicyInvalidator::PolicyInvalidationHandler::
    HaveInvalidationsBeenEnabledForAWhileForMetricsRecording() {
  CHECK(invalidations_enabled_time_);
  // If invalidations have been enabled for less than the grace period, then
  // consider invalidations to be disabled for metrics reporting.
  const base::TimeDelta elapsed =
      clock_->Now() - invalidations_enabled_time_.value();
  return elapsed.InSeconds() >= kInvalidationGracePeriod;
}

int64_t CloudPolicyInvalidator::highest_handled_invalidation_version() const {
  return policy_invalidation_handler_.highest_handled_invalidation_version();
}

invalidation::InvalidationService*
CloudPolicyInvalidator::invalidation_service_for_test() const {
  if (std::holds_alternative<InvalidationServicePtr>(
          invalidation_service_or_listener_)) {
    return std::get<InvalidationServicePtr>(invalidation_service_or_listener_);
  }
  return nullptr;
}

void CloudPolicyInvalidator::PolicyInvalidationHandler::
    CancelInvalidationHandlingIfWaitingForOne() {
  if (invalid_) {
    AcknowledgeInvalidation();
  }
}

void CloudPolicyInvalidator::PolicyInvalidationHandler::
    CancelInvalidationHandling() {
  weak_factory_.InvalidateWeakPtrs();
}

}  // namespace policy
