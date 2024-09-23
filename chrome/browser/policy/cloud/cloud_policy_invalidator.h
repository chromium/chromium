// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_CLOUD_POLICY_INVALIDATOR_H_
#define CHROME_BROWSER_POLICY_CLOUD_CLOUD_POLICY_INVALIDATOR_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_invalidation_scope.h"

namespace base {
class Clock;
class SequencedTaskRunner;
}

namespace invalidation {
class InvalidationService;
}

namespace policy {

// The invalidation service.
using InvalidationServicePtr = raw_ptr<invalidation::InvalidationService>;
using InvalidationListenerPtr = raw_ptr<invalidation::InvalidationListener>;

// The state of the object.
enum class State { UNINITIALIZED, STOPPED, STARTED, SHUT_DOWN };

// Listens for and provides policy invalidations.
class CloudPolicyInvalidator
    : public invalidation::InvalidationHandler,
      public invalidation::InvalidationListener::Observer,
      public CloudPolicyCore::Observer,
      public CloudPolicyStore::Observer {
 public:
  // The number of minutes to delay a policy refresh after receiving an
  // invalidation with no payload.
  static const int kMissingPayloadDelay;

  // The default, min and max values for max_fetch_delay_.
  static const int kMaxFetchDelayDefault;
  static const int kMaxFetchDelayMin;
  static const int kMaxFetchDelayMax;

  // The grace period, in seconds, to allow for invalidations to be received
  // once the invalidation service starts up.
  static const int kInvalidationGracePeriod;

  // Returns a name of a refresh metric associated with the given scope.
  static const char* GetPolicyRefreshMetricName(PolicyInvalidationScope scope);
  // Returns a name of an invalidation metric associated with the given scope.
  static const char* GetPolicyInvalidationMetricName(
      PolicyInvalidationScope scope);

  // |scope| indicates the invalidation scope that this invalidator
  // is responsible for.
  // |core| is the cloud policy core which connects the various policy objects.
  // It must remain valid until Shutdown is called.
  // |task_runner| is used for scheduling delayed tasks. It must post tasks to
  // the main policy thread.
  // |clock| is used to get the current time.
  // |highest_handled_invalidation_version| is the highest invalidation version
  // that was handled already before this invalidator was created.
  // |device_local_account_id| is a unique identity for invalidator with
  // DeviceLocalAccount |scope| to have unique owner name. May be let empty
  // if scope is not DeviceLocalAccount.
  // TODO(b/341376574): Add unit tests for `invalidation::InvalidationListener`
  // setup.
  CloudPolicyInvalidator(
      PolicyInvalidationScope scope,
      CloudPolicyCore* core,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      base::Clock* clock,
      int64_t highest_handled_invalidation_version,
      const std::string& device_local_account_id);
  CloudPolicyInvalidator(
      PolicyInvalidationScope scope,
      CloudPolicyCore* core,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      base::Clock* clock,
      int64_t highest_handled_invalidation_version);
  CloudPolicyInvalidator(const CloudPolicyInvalidator&) = delete;
  CloudPolicyInvalidator& operator=(const CloudPolicyInvalidator&) = delete;
  ~CloudPolicyInvalidator() override;

  // Initializes the invalidator. No invalidations will be generated before this
  // method is called. This method must only be called once.
  // `invalidation_service` or `invalidation_listener` is the invalidation
  // service to use and must remain valid until Shutdown is called.
  void Initialize(std::variant<invalidation::InvalidationService*,
                               invalidation::InvalidationListener*>
                      invalidation_service_or_listener);

  // Shuts down and disables invalidations. It must be called before the object
  // is destroyed.
  void Shutdown();

  // The highest invalidation version that was handled already.
  int64_t highest_handled_invalidation_version() const;

  invalidation::InvalidationService* invalidation_service_for_test() const;

  // invalidation::InvalidationHandler:
  void OnInvalidatorStateChange(invalidation::InvalidatorState state) override;
  void OnIncomingInvalidation(
      const invalidation::Invalidation& invalidation) override;
  std::string GetOwnerName() const override;
  bool IsPublicTopic(const invalidation::Topic& topic) const override;

  // CloudPolicyCore::Observer:
  void OnCoreConnected(CloudPolicyCore* core) override;
  void OnRefreshSchedulerStarted(CloudPolicyCore* core) override;
  void OnCoreDisconnecting(CloudPolicyCore* core) override;

  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* store) override;
  void OnStoreError(CloudPolicyStore* store) override;

  // InvalidationListener::Observer:
  void OnExpectationChanged(
      invalidation::InvalidationsExpected expected) override;
  void OnInvalidationReceived(
      const invalidation::DirectInvalidation& invalidation) override;
  std::string GetType() const override;

 private:
  // Handles policy refresh depending on invalidations availability and incoming
  // invalidations.
  class PolicyInvalidationHandler {
   public:
    PolicyInvalidationHandler(
        PolicyInvalidationScope scope,
        int64_t highest_handled_invalidation_version,
        CloudPolicyCore* core,
        base::Clock* clock,
        scoped_refptr<base::SequencedTaskRunner> task_runner);

    ~PolicyInvalidationHandler();

    // Handles an invalidation to the policy.
    void HandleInvalidation(const invalidation::Invalidation& invalidation);

    // Informs the core's refresh scheduler about whether invalidations are
    // enabled.
    void UpdateInvalidationsEnabled(bool invalidations_enabled);

    // Update |max_fetch_delay_| based on the given policy map.
    void UpdateMaxFetchDelay(const PolicyMap& policy_map);

    void HandlePolicyRefresh(CloudPolicyStore* store,
                             bool is_registered_for_invalidations,
                             bool invalidations_enabled);

    // Cancels ongoing policy refresh if any.
    void CancelInvalidationHandlingIfWaitingForOne();

    void CancelInvalidationHandling();

    int64_t highest_handled_invalidation_version() const {
      return highest_handled_invalidation_version_;
    }

   private:
    void set_max_fetch_delay(int delay);

    // Refresh the policy.
    // |is_missing_payload| is set to true if the callback is being invoked in
    // response to an invalidation with a missing payload.
    void RefreshPolicy(bool is_missing_payload);

    // Acknowledge the latest invalidation.
    void AcknowledgeInvalidation();

    // Determine if invalidations have been enabled longer than the grace
    // period.
    // This is a heuristic attempt to avoid counting initial policy fetches as
    // invalidation-triggered.
    // See https://codereview.chromium.org/213743014 for more details.
    bool HaveInvalidationsBeenEnabledForAWhileForMetricsRecording();

    // The invalidation scope this invalidator is responsible for.
    const PolicyInvalidationScope scope_;

    // The cloud policy core.
    raw_ptr<CloudPolicyCore> core_;

    // The time that invalidations became enabled.
    std::optional<base::Time> invalidations_enabled_time_;

    // Whether the policy is current invalid. This is set to true when an
    // invalidation is received and reset when the policy fetched due to the
    // invalidation is stored.
    bool invalid_ = false;

    // The version of the latest invalidation received. This is compared to
    // the invalidation version of policy stored to determine when the
    // invalidated policy is up to date.
    int64_t invalidation_version_ = 0;

    // The highest invalidation version that was handled already.
    int64_t highest_handled_invalidation_version_;

    // The hash value of the current policy. This is used to determine if a new
    // policy is different from the current one.
    uint32_t policy_hash_value_ = 0;

    // The maximum random delay, in ms, between receiving an invalidation and
    // fetching the new policy.
    int max_fetch_delay_ = kMaxFetchDelayDefault;

    // The clock.
    const raw_ptr<base::Clock> clock_;

    // Schedules delayed tasks.
    const scoped_refptr<base::SequencedTaskRunner> task_runner_;

    // A thread checker to make sure that callbacks are invoked on the correct
    // thread.
    THREAD_CHECKER(thread_checker_);

    // WeakPtrFactory used to create callbacks to this object.
    base::WeakPtrFactory<PolicyInvalidationHandler> weak_factory_{this};
  };

  // Returns true if `this` is observing `invalidation_service_` or
  // `invalidation_listener_`.
  bool IsRegistered() const;

  // Returns true if `IsRegistered()` and `invalidation_service_` or
  // `invalidation_listener_` is enabled.
  bool AreInvalidationsEnabled() const;

  // Update topic subscription with the invalidation service based on the
  // given policy data.
  void UpdateSubscriptionWithInvalidationService(
      const enterprise_management::PolicyData* policy);

  // The event executed when the store is loaded.
  void HandleOnStoreLoadedForListener(
      invalidation::InvalidationListener* listener);

  // Registers this handler with |invalidation_service_| if needed and
  // subscribes to the given |topic| with the invalidation service.
  void RegisterWithInvalidationService(const invalidation::Topic& topic);

  // Unregisters this handler and unsubscribes from the current topic with
  // the invalidation service.
  // TODO(crbug.com/40676667): Topic subscriptions remain active after browser
  // restart, so explicit unsubscription here causes redundant (un)subscription
  // traffic (and potentially leaking subscriptions).
  void UnregisterWithInvalidationService();

  State state_;

  PolicyInvalidationHandler policy_invalidation_handler_;

  // The invalidation scope this invalidator is responsible for.
  const PolicyInvalidationScope scope_;

  // The unique name to be returned with by GetOwnerName().
  // TODO(b/341376574): Remove once does not implement
  // `invalidation::InvalidationHandler`.
  const std::string owner_name_;

  // The cloud policy core.
  raw_ptr<CloudPolicyCore> core_;

  base::ScopedObservation<CloudPolicyCore, CloudPolicyInvalidator>
      core_observation_{this};
  base::ScopedObservation<CloudPolicyStore, CloudPolicyInvalidator>
      store_observation_{this};

  std::variant<InvalidationServicePtr, InvalidationListenerPtr>
      invalidation_service_or_listener_ =
          static_cast<InvalidationServicePtr>(nullptr);

  invalidation::InvalidationsExpected are_invalidations_expected_ =
      invalidation::InvalidationsExpected::kMaybe;

  base::ScopedObservation<invalidation::InvalidationListener,
                          CloudPolicyInvalidator>
      invalidation_listener_observation_{this};
  base::ScopedObservation<invalidation::InvalidationService,
                          CloudPolicyInvalidator>
      invalidation_service_observation_{this};

  // The topic representing the policy in the invalidation service.
  invalidation::Topic topic_;
  const std::string device_local_account_id_;

  // A thread checker to make sure that callbacks are invoked on the correct
  // thread.
  THREAD_CHECKER(thread_checker_);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_CLOUD_POLICY_INVALIDATOR_H_
