// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_STATUS_PROVIDER_CLOUD_POLICY_CORE_STATUS_PROVIDER_H_
#define CHROME_BROWSER_POLICY_STATUS_PROVIDER_CLOUD_POLICY_CORE_STATUS_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

namespace policy {
class CloudPolicyCore;
}  // namespace policy

// Status provider implementation that pulls cloud policy status up to two
// CloudPolicyCore instances provided at construction time. Also listens for
// changes on those CloudPolicyCores and reports them through the status change
// callback.
class CloudPolicyCoreStatusProvider
    : public policy::PolicyStatusProvider,
      public policy::CloudPolicyStore::Observer {
 public:
  explicit CloudPolicyCoreStatusProvider(
      policy::CloudPolicyCore* core,
      policy::CloudPolicyCore* extension_install_core);

  CloudPolicyCoreStatusProvider(const CloudPolicyCoreStatusProvider&) = delete;
  CloudPolicyCoreStatusProvider& operator=(
      const CloudPolicyCoreStatusProvider&) = delete;

  ~CloudPolicyCoreStatusProvider() override;

  // policy::CloudPolicyStore::Observer implementation.
  void OnStoreLoaded(policy::CloudPolicyStore* store) override;
  void OnStoreError(policy::CloudPolicyStore* store) override;
  void OnStoreDestruction(policy::CloudPolicyStore* store) override;

 protected:
  // Policy status is read from the CloudPolicyClient, CloudPolicyStore and
  // CloudPolicyRefreshScheduler hosted by this |core_|. core_ cannot be null.
  raw_ptr<policy::CloudPolicyCore> core_;
  // Policy status is read from the extension install CloudPolicyCore instance.
  // extension_install_core_ can be null.
  raw_ptr<policy::CloudPolicyCore> extension_install_core_;

 private:
  base::ScopedObservation<policy::CloudPolicyStore,
                          policy::CloudPolicyStore::Observer>
      scoped_observation_{this};
  base::ScopedObservation<policy::CloudPolicyStore,
                          policy::CloudPolicyStore::Observer>
      extension_install_core_scoped_observation_{this};
};

#endif  // CHROME_BROWSER_POLICY_STATUS_PROVIDER_CLOUD_POLICY_CORE_STATUS_PROVIDER_H_
