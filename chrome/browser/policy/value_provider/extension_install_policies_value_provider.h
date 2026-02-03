// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_VALUE_PROVIDER_EXTENSION_INSTALL_POLICIES_VALUE_PROVIDER_H_
#define CHROME_BROWSER_POLICY_VALUE_PROVIDER_EXTENSION_INSTALL_POLICIES_VALUE_PROVIDER_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/policy/cloud/extension_install_policy_service.h"
#include "chrome/browser/policy/value_provider/policy_value_provider.h"

// Returns the names and values of extension install policies.
class ExtensionInstallPoliciesValueProvider
    : public policy::PolicyValueProvider,
      public policy::ExtensionInstallPolicyService::Observer {
 public:
  explicit ExtensionInstallPoliciesValueProvider(
      Profile* profile,
      policy::ExtensionInstallPolicyService* service);
  ~ExtensionInstallPoliciesValueProvider() override;

  // policy::PolicyValueProvider implementation.
  base::DictValue GetValues() override;
  base::DictValue GetNames() override;

  // policy::ExtensionInstallPolicyService::Observer implementation.
  void OnExtensionInstallPolicyUpdated() override;

 private:
  raw_ref<Profile> profile_;
  base::ScopedObservation<policy::ExtensionInstallPolicyService,
                          policy::ExtensionInstallPolicyService::Observer>
      observation_{this};
};

#endif  // CHROME_BROWSER_POLICY_VALUE_PROVIDER_EXTENSION_INSTALL_POLICIES_VALUE_PROVIDER_H_
