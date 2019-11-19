// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_CLOUD_POLICY_INVALIDATOR_H_
#define CHROME_BROWSER_POLICY_CLOUD_CLOUD_POLICY_INVALIDATOR_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "google/cacheinvalidation/include/types.h"

namespace base {
class Clock;
class SequencedTaskRunner;
}

namespace invalidation {
class InvalidationService;
}

namespace policy {

// Listens for and provides policy invalidations.
class CloudPolicyInvalidator : public syncer::InvalidationHandler,
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

  // Time, in seconds, for which unknown version invalidations are ignored after
  // fetching a policy.
  static const int kUnknownVersionIgnorePeriod;

  // The max tolerated discrepancy, in seconds, between policy timestamps and
  // invalidation timestamps when determining if an invalidation is expired.
  static const int kMaxInvalidationTimeDelta;

  // |type| indicates the policy type that this invalidator is responsible for.
  // |core| is the cloud policy core which connects the various policy objects.
  // It must remain valid until Shutdown is called.
  // |task_runner| is used for scheduling delayed tasks. It must post tasks to
  // the main policy thread.
  // |clock| is used to get the current time.
  // |highest_handled_invalidation_version| is the highest invalidation version
  // that was handled already before this invalidator was created.
  CloudPolicyInvalidator(
      enterprise_management::DeviceRegisterRequest::Type type,
      CloudPolicyCore* core,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      base::Clock* clock,
      int64_t highest_handled_invalidation_version);
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

  // syncer::InvalidationHandler:
  void OnInvalidatorStateChange(syncer::InvalidatorState state) override;
  void OnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) override;
  std::string GetOwnerName() const override;
  bool IsPublicTopic(const syncer::Topic& topic) const override;

  // CloudPolicyCore::Observer:
  void OnCoreConnected(CloudPolicyCore* core) override;
  void OnRefreshSchedulerStarted(CloudPolicyCore* core) override;
  void OnCoreDisconnecting(CloudPolicyCore* core) override;

  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* store) override;
  void OnStoreError(CloudPolicyStore* store) override;

 private:
  // Handle an invalidation to the policy.
  void HandleInvalidation(const syncer::Invalidation& invalidation);

  // Update object registration with the invalidation service based on the
  // given policy data.
  void UpdateRegistration(const enterprise_management::PolicyData* policy);

  // Registers the given object with the invalidation service.
  void Register(const invalidation::ObjectId& object_id);

  // Unregisters the current object with the invalidation service.
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

  // Determine if an invalidation has expired.
  // |version| is the version of the invalidation, or zero for unknown.
  bool IsInvalidationExpired(int64_t version);

  // Get the kMetricPolicyInvalidations histogram metric which should be
  // incremented when an invalidation is received.
  PolicyInvalidationType GetInvalidationMetric(bool is_missing_payload,
                                               bool is_expired);

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

  // The policy type this invalidator is responsible for.
  const enterprise_management::DeviceRegisterRequest::Type type_;

  // The cloud policy core.
  CloudPolicyCore* core_;

  // Schedules delayed tasks.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The clock.
  base::Clock* clock_;

  // The invalidation service.
  invalidation::InvalidationService* invalidation_service_;

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

  // The object id representing the policy in the invalidation service.
  invalidation::ObjectId object_id_;

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
  std::unique_ptr<syncer::Invalidation> invalidation_;

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

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyInvalidator);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_CLOUD_POLICY_INVALIDATOR_H_
