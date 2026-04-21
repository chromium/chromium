// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_STATUS_PROVIDER_CLOUD_POLICY_CORE_STATUS_PROVIDER_H_
#define CHROME_BROWSER_POLICY_STATUS_PROVIDER_CLOUD_POLICY_CORE_STATUS_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/policy/cloud/extension_install_policy_service.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

namespace policy {
class CloudPolicyCore;
class CloudPolicyManager;
class CloudPolicyStore;
}  // namespace policy

class Profile;

// Status provider implementation that pulls cloud policy status up to two
// CloudPolicyCore instances provided at construction time. Also listens for
// changes on those CloudPolicyCores and reports them through the status change
// callback.
class CloudPolicyCoreStatusProvider
    : public policy::PolicyStatusProvider,
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
      public policy::ExtensionInstallPolicyService::Observer,
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
      public policy::CloudPolicyStore::Observer {
 public:
  CloudPolicyCoreStatusProvider(
      policy::CloudPolicyManager* cloud_policy_manager,
      Profile* profile);

  CloudPolicyCoreStatusProvider(const CloudPolicyCoreStatusProvider&) = delete;
  CloudPolicyCoreStatusProvider& operator=(
      const CloudPolicyCoreStatusProvider&) = delete;

  ~CloudPolicyCoreStatusProvider() override;

  // policy::CloudPolicyStore::Observer implementation.
  void OnStoreLoaded(policy::CloudPolicyStore* store) override;
  void OnStoreError(policy::CloudPolicyStore* store) override;
  void OnStoreDestruction(policy::CloudPolicyStore* store) override;

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // policy::ExtensionInstallPolicyService::Observer implementation.
  void OnManagerInitializationComplete(
      policy::CloudPolicyManager* manager) override;
  void OnExtensionInstallPolicyUpdated() override;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

 protected:
  // Policy status is read from the CloudPolicyClient, CloudPolicyStore and
  // CloudPolicyRefreshScheduler hosted by this |core_|. core_ cannot be null.
  policy::CloudPolicyCore* core();
  // Policy status is read from the extension install CloudPolicyCore instance.
  // extension_install_core_ can be null.
  policy::CloudPolicyCore* extension_install_core();

  Profile* profile() { return profile_; }

 private:
  raw_ptr<policy::CloudPolicyManager> cloud_policy_manager_;
  raw_ptr<Profile> profile_;
  base::ScopedObservation<policy::CloudPolicyStore,
                          policy::CloudPolicyStore::Observer>
      scoped_observation_{this};
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  base::ScopedObservation<policy::CloudPolicyStore,
                          policy::CloudPolicyStore::Observer>
      extension_install_core_scoped_observation_{this};
  base::ScopedObservation<policy::ExtensionInstallPolicyService,
                          policy::ExtensionInstallPolicyService::Observer>
      extension_install_policy_service_scoped_observation_{this};
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
};

#endif  // CHROME_BROWSER_POLICY_STATUS_PROVIDER_CLOUD_POLICY_CORE_STATUS_PROVIDER_H_
