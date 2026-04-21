// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/cloud_policy_core_status_provider.h"

#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/policy/cloud/extension_install_policy_service.h"
#include "chrome/browser/policy/cloud/extension_install_policy_service_factory.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

CloudPolicyCoreStatusProvider::CloudPolicyCoreStatusProvider(
    policy::CloudPolicyManager* cloud_policy_manager,
    Profile* profile)
    : cloud_policy_manager_(cloud_policy_manager), profile_(profile) {
  CHECK(cloud_policy_manager_);
  scoped_observation_.Observe(core()->store());
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (extension_install_core() && extension_install_core()->store()) {
    extension_install_core_scoped_observation_.Observe(
        extension_install_core()->store());
  } else if (auto* extension_install_policy_service =
                 policy::ExtensionInstallPolicyServiceFactory::
                     GetForBrowserContext(profile)) {
    if (extension_install_policy_service) {
      extension_install_policy_service_scoped_observation_.Observe(
          extension_install_policy_service);
    }
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

  // TODO(bartfab): Add an observer that watches for client errors. Observing
  // core_->client() directly is not safe as the client may be destroyed and
  // (re-)created anytime if the user signs in or out on desktop platforms.
}

CloudPolicyCoreStatusProvider::~CloudPolicyCoreStatusProvider() = default;

void CloudPolicyCoreStatusProvider::OnStoreLoaded(
    policy::CloudPolicyStore* store) {
  NotifyStatusChange();
}

void CloudPolicyCoreStatusProvider::OnStoreError(
    policy::CloudPolicyStore* store) {
  NotifyStatusChange();
}

void CloudPolicyCoreStatusProvider::OnStoreDestruction(
    policy::CloudPolicyStore* store) {
  scoped_observation_.Reset();
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (extension_install_core()) {
    extension_install_core_scoped_observation_.Reset();
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
void CloudPolicyCoreStatusProvider::OnManagerInitializationComplete(
    policy::CloudPolicyManager* manager) {
  // There are 2 possible managers, the profile level and CBCM one. We only care
  // about the one tracked by this instance.
  if (manager != cloud_policy_manager_) {
    return;
  }

  // No need to observe the extension install policy service anymore, as the
  // manager is now initialized.
  extension_install_policy_service_scoped_observation_.Reset();

  // Observe the extension install core store.
  CHECK(!extension_install_core_scoped_observation_.IsObserving());
  if (extension_install_core() && extension_install_core()->store()) {
    extension_install_core_scoped_observation_.Observe(
        extension_install_core()->store());
  }
}

void CloudPolicyCoreStatusProvider::OnExtensionInstallPolicyUpdated() {}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

policy::CloudPolicyCore* CloudPolicyCoreStatusProvider::core() {
  return cloud_policy_manager_->core();
}

policy::CloudPolicyCore*
CloudPolicyCoreStatusProvider::extension_install_core() {
  return cloud_policy_manager_->extension_install_core();
}
