// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_EXTENSION_INSTALL_POLICY_INVALIDATOR_H_
#define CHROME_BROWSER_POLICY_CLOUD_EXTENSION_INSTALL_POLICY_INVALIDATOR_H_

#include <memory>
#include <string>

#include "chrome/browser/policy/cloud/policy_invalidator.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/policy/core/common/cloud/policy_invalidation_scope.h"

namespace base {
class Clock;
class SequencedTaskRunner;
}  // namespace base

namespace policy {

class CloudPolicyCore;
class CloudPolicyStore;

// PolicyInvalidator for extension install policies.
class ExtensionInstallPolicyInvalidator : public PolicyInvalidator {
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
  ExtensionInstallPolicyInvalidator(
      PolicyInvalidationScope scope,
      invalidation::InvalidationListener* invalidation_listener,
      CloudPolicyCore* core,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const base::Clock* clock,
      const std::string& device_local_account_id = std::string());
  ExtensionInstallPolicyInvalidator(const ExtensionInstallPolicyInvalidator&) =
      delete;
  ExtensionInstallPolicyInvalidator& operator=(
      const ExtensionInstallPolicyInvalidator&) = delete;
  ~ExtensionInstallPolicyInvalidator() override;

  std::string GetType() const override;

 private:
  // Handles policy refresh depending on invalidations availability and incoming
  // invalidations.
  class ExtensionInstallPolicyInvalidationHandler
      : public PolicyInvalidator::PolicyInvalidationHandler {
   public:
    ExtensionInstallPolicyInvalidationHandler(
        PolicyInvalidationScope scope,
        CloudPolicyCore* core,
        const base::Clock* clock,
        scoped_refptr<base::SequencedTaskRunner> task_runner);

    ~ExtensionInstallPolicyInvalidationHandler() override;

    const char* GetPolicyRefreshMetricName(
        PolicyInvalidationScope scope) override;
    const char* GetPolicyInvalidationMetricName(
        PolicyInvalidationScope scope) override;
  };
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_EXTENSION_INSTALL_POLICY_INVALIDATOR_H_
