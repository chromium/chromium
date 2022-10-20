// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_CLOUD_POLICY_INVALIDATOR_H_
#define CHROME_BROWSER_POLICY_CLOUD_CLOUD_POLICY_INVALIDATOR_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
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

// Listens for and provides policy invalidations.
class CloudPolicyInvalidator : public invalidation::InvalidationHandler,
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
  // Returns a name of a FCM refresh metric associated with the given scope.
  static const char* GetPolicyRefreshFcmMetricName(
      PolicyInvalidationScope scope);
  // Returns a name of an invalidation metric associated with the given scope.
  static const char* GetPolicyInvalidationMetricName(
      PolicyInvalidationScope scope);
  // Returns a name of an FCM invalidation metric associated with the given
  // scope.
  static const char* GetPolicyInvalidationFcmMetricName(
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
  // |invalidation_service| is the invalidation service to use and must remain
  // valid until Shutdown is called.
  void Initialize(invalidation::InvalidationService* invalidation_service);

  // Shuts down and disables invalidations. It must be called before the object
  // is destroyed.
  void Shutdown();

  // Whether the invalidator currently has the ability to receive invalidations.
  bool invalidations_enabled() {
    return invalidations_enabled_;
  }

  // The highest invalidation version that was handled already.
  int64_t highest_handled_invalidation_version() const {
    return highest_handled_invalidation_version_;
  }

  invalidation::InvalidationService* invalidation_service_for_test() const {
    return invalidation_service_;
  }

  // invalidation::InvalidationHandler:
  void OnInvalidatorStateChange(invalidation::InvalidatorState state) override;
  void OnIncomingInvalidation(
      const invalidation::TopicInvalidationMap& invalidation_map) override;
  std::string GetOwnerName() const override;
  bool IsPublicTopic(const invalidation::Topic& topic) const override;

  // CloudPolicyCore::Observer:
  void OnCoreConnected(CloudPolicyCore* core) override;
  void OnRefreshSchedulerStarted(CloudPolicyCore* core) override;
  void OnCoreDisconnecting(CloudPolicyCore* core) override;

  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* store) override;
  void OnStoreError(CloudPolicyStore* store) override;

 private:
  // Handle an invalidation to the policy.
  void HandleInvalidation(const invalidation::Invalidation& invalidation);

  // Update topic subscription with the invalidation service based on the
  // given policy data.
  void UpdateSubscription(const enterprise_management::PolicyData* policy);

  // Registers this handler with |invalidation_service_| if needed and
  // subscribes to the given |topic| with the invalidation service.
  void Register(const invalidation::Topic& topic);

  // Unregisters this handler and unsubscribes from the current topic with
  // the invalidation service.
  // TODO(crbug.com/1056114): Topic subscriptions remain active after browser
  // restart, so explicit unsubscription here causes redundant (un)subscription
  // traffic (and potentially leaking subscriptions).
  void Unregister();

  // Update |max_fetch_delay_| based on the given policy map.
  void UpdateMaxFetchDelay(const PolicyMap& policy_map);
  void set_max_fetch_delay(int delay);

  // Updates invalidations_enabled_ and calls the invalidation handler if the
  // value changed.
  void UpdateInvalidationsEnabled();

  // Refresh the policy.
  // |is_missing_payload| is set to true if the callback is being invoked in
  // response to an invalidation with a missing payload.
  void RefreshPolicy(bool is_missing_payload);

  // Acknowledge the latest invalidation.
  void AcknowledgeInvalidation();

  // Determines if the given policy is different from the policy passed in the
  // previous call.
  bool IsPolicyChanged(const enterprise_management::PolicyData* policy);

  // Determine if invalidations have been enabled longer than the grace period.
  bool GetInvalidationsEnabled();

  // The state of the object.
  enum State {
    UNINITIALIZED,
    STOPPED,
    STARTED,
    SHUT_DOWN
  };
  State state_;

  // The invalidation scope this invalidator is responsible for.
  const PolicyInvalidationScope scope_;

  // The unique name to be returned with by GetOwnerName().
  const std::string owner_name_;

  // The cloud policy core.
  raw_ptr<CloudPolicyCore> core_;

  // Schedules delayed tasks.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The clock.
  raw_ptr<base::Clock> clock_;

  // The invalidation service.
  raw_ptr<invalidation::InvalidationService, DanglingUntriaged>
      invalidation_service_;

  // Whether the invalidator currently has the ability to receive invalidations.
  // This is true if the invalidation service is enabled and the invalidator
  // has registered for a policy object.
  bool invalidations_enabled_;

  // The time that invalidations became enabled.
  base::Time invalidations_enabled_time_;

  // Whether the invalidation service is currently enabled.
  bool invalidation_service_enabled_;

  // Whether this object has registered for policy invalidations.
  bool is_registered_;

  // The topic representing the policy in the invalidation service.
  invalidation::Topic topic_;

  // Whether the policy is current invalid. This is set to true when an
  // invalidation is received and reset when the policy fetched due to the
  // invalidation is stored.
  bool invalid_;

  // The version of the latest invalidation received. This is compared to
  // the invalidation version of policy stored to determine when the
  // invalidated policy is up to date.
  int64_t invalidation_version_;

  // The number of invalidations with unknown version received. Since such
  // invalidations do not provide a version number, this count is used to set
  // invalidation_version_ when such invalidations occur.
  int unknown_version_invalidation_count_;

  // The highest invalidation version that was handled already.
  int64_t highest_handled_invalidation_version_;

  // The most up to date invalidation.
  std::unique_ptr<invalidation::Invalidation> invalidation_;

  // The maximum random delay, in ms, between receiving an invalidation and
  // fetching the new policy.
  int max_fetch_delay_;

  // The hash value of the current policy. This is used to determine if a new
  // policy is different from the current one.
  uint32_t policy_hash_value_;

  // A thread checker to make sure that callbacks are invoked on the correct
  // thread.
  base::ThreadChecker thread_checker_;

  // WeakPtrFactory used to create callbacks to this object.
  base::WeakPtrFactory<CloudPolicyInvalidator> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_CLOUD_POLICY_INVALIDATOR_H_
