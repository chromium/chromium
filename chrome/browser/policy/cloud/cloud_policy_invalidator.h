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
#include "chrome/browser/policy/cloud/policy_invalidator.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_invalidation_scope.h"

namespace base {
class Clock;
class SequencedTaskRunner;
}

namespace policy {

// PolicyInvalidator for Chrome browser policies.
class CloudPolicyInvalidator : public PolicyInvalidator {
 public:
  // Returns a name of a refresh metric associated with the given scope.
  static const char* GetPolicyRefreshMetricName(PolicyInvalidationScope scope);
  // Returns a name of an invalidation metric associated with the given scope.
  static const char* GetPolicyInvalidationMetricName(
      PolicyInvalidationScope scope);

  // |scope| indicates the invalidation scope that this invalidator
  // is responsible for.
  // |invalidation_listener| provides invalidations and is observed during the
  // whole invalidator's lifetime. Must remain valid until the invalidator is
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
      const base::Clock* clock,
      const std::string& device_local_account_id = std::string());
  CloudPolicyInvalidator(const CloudPolicyInvalidator&) = delete;
  CloudPolicyInvalidator& operator=(const CloudPolicyInvalidator&) = delete;
  ~CloudPolicyInvalidator() override;

  // InvalidationListener::Observer:
  std::string GetType() const override;

 protected:
  // Handles policy refresh depending on invalidations availability and incoming
  // invalidations.
  class CloudPolicyInvalidationHandler
      : public PolicyInvalidator::PolicyInvalidationHandler {
   public:
    CloudPolicyInvalidationHandler(
        PolicyInvalidationScope scope,
        CloudPolicyCore* core,
        const base::Clock* clock,
        scoped_refptr<base::SequencedTaskRunner> task_runner);

    ~CloudPolicyInvalidationHandler() override;
    const char* GetPolicyRefreshMetricName(
        PolicyInvalidationScope scope) override;
    const char* GetPolicyInvalidationMetricName(
        PolicyInvalidationScope scope) override;
  };
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_CLOUD_POLICY_INVALIDATOR_H_
