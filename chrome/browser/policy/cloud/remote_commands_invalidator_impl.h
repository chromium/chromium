// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_REMOTE_COMMANDS_INVALIDATOR_IMPL_H_
#define CHROME_BROWSER_POLICY_CLOUD_REMOTE_COMMANDS_INVALIDATOR_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/policy/cloud/remote_commands_invalidator.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_invalidation_scope.h"

namespace base {
class Clock;
}

namespace policy {

// Implementation of invalidator for remote commands services. This class
// listens to events from CloudPolicyCore and CloudPolicyStore and builds
// with RemoteCommandsInvalidator to complete the tasks.
// TODO(crbug.com/1319443): Merge with RemoteCommandsInvalidator.
class RemoteCommandsInvalidatorImpl : public RemoteCommandsInvalidator,
                                      public CloudPolicyCore::Observer,
                                      public CloudPolicyStore::Observer {
 public:
  RemoteCommandsInvalidatorImpl(CloudPolicyCore* core,
                                base::Clock* clock,
                                PolicyInvalidationScope scope);
  RemoteCommandsInvalidatorImpl(const RemoteCommandsInvalidatorImpl&) = delete;
  RemoteCommandsInvalidatorImpl& operator=(
      const RemoteCommandsInvalidatorImpl&) = delete;

  // RemoteCommandsInvalidator:
  void OnInitialize() override;
  void OnShutdown() override;
  void OnStart() override;
  void OnStop() override;
  void DoRemoteCommandsFetch(
      const invalidation::Invalidation& invalidation) override;

  // CloudPolicyCore::Observer:
  void OnCoreConnected(CloudPolicyCore* core) override;
  void OnRefreshSchedulerStarted(CloudPolicyCore* core) override;
  void OnCoreDisconnecting(CloudPolicyCore* core) override;
  void OnRemoteCommandsServiceStarted(CloudPolicyCore* core) override;

  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* store) override;
  void OnStoreError(CloudPolicyStore* store) override;

 private:
  void RecordInvalidationMetric(
      const invalidation::Invalidation& invalidation) const;

  const raw_ptr<CloudPolicyCore> core_;

  const raw_ptr<const base::Clock> clock_;

  const PolicyInvalidationScope scope_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_REMOTE_COMMANDS_INVALIDATOR_IMPL_H_
