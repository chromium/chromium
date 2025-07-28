// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_CLOUD_POLICY_INVALIDATOR_H_
#define CHROME_BROWSER_POLICY_CLOUD_CLOUD_POLICY_INVALIDATOR_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_invalidation_scope.h"

namespace base {
class Clock;
class SequencedTaskRunner;
}

namespace policy {

// Listens for and provides policy invalidations.
class CloudPolicyInvalidator
    : public invalidation::InvalidationListener::Observer,
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
  // |invalidation_listener| provides invalidations and is observed during the
  // whole invaldiator's lifetime. Must remain valid until the invalidator is
  // destroyed.
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
  CloudPolicyInvalidator(
      PolicyInvalidationScope scope,
      invalidation::InvalidationListener* invalidation_listener,
      CloudPolicyCore* core,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      base::Clock* clock,
      int64_t highest_handled_invalidation_version,
      const std::string& device_local_account_id);
  CloudPolicyInvalidator(
      PolicyInvalidationScope scope,
      invalidation::InvalidationListener* invalidation_listener,
      CloudPolicyCore* core,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      base::Clock* clock,
      int64_t highest_handled_invalidation_version);
  CloudPolicyInvalidator(const CloudPolicyInvalidator&) = delete;
  CloudPolicyInvalidator& operator=(const CloudPolicyInvalidator&) = delete;
  ~CloudPolicyInvalidator() override;

  // The highest invalidation version that was handled already.
  int64_t highest_handled_invalidation_version() const;

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
    void HandleInvalidation(
        const invalidation::DirectInvalidation& invalidation);

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

    bool IsCoreReady() const {
      return core_->IsConnected() && core_->refresh_scheduler() &&
             core_->store();
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
    const raw_ptr<CloudPolicyCore> core_;

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

  // Returns true if ready to receive invalidations.
  bool IsRegistered() const;

  // Returns true if ready to receive invalidations and invalidations are
  // enabled.
  bool AreInvalidationsEnabled() const;

  PolicyInvalidationHandler policy_invalidation_handler_;

  // The invalidation scope this invalidator is responsible for.
  const PolicyInvalidationScope scope_;

  // The cloud policy core.
  raw_ptr<CloudPolicyCore> core_;

  base::ScopedObservation<CloudPolicyCore, CloudPolicyInvalidator>
      core_observation_{this};
  base::ScopedObservation<CloudPolicyStore, CloudPolicyInvalidator>
      store_observation_{this};

  raw_ptr<invalidation::InvalidationListener> invalidation_listener_;

  invalidation::InvalidationsExpected are_invalidations_expected_ =
      invalidation::InvalidationsExpected::kMaybe;

  base::ScopedObservation<invalidation::InvalidationListener,
                          CloudPolicyInvalidator>
      invalidation_listener_observation_{this};

  const std::string device_local_account_id_;

  // A thread checker to make sure that callbacks are invoked on the correct
  // thread.
  THREAD_CHECKER(thread_checker_);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_CLOUD_POLICY_INVALIDATOR_H_
