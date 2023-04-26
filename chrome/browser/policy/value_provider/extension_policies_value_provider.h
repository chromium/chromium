// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_VALUE_PROVIDER_EXTENSION_POLICIES_VALUE_PROVIDER_H_
#define CHROME_BROWSER_POLICY_VALUE_PROVIDER_EXTENSION_POLICIES_VALUE_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/policy/value_provider/policy_value_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension.h"

namespace content {
class BrowserContext;
}  // namespace content

// Returns the extension policy values.
class ExtensionPoliciesValueProvider
    : public policy::PolicyValueProvider,
      public extensions::ExtensionRegistryObserver,
      public policy::PolicyService::Observer {
 public:
  explicit ExtensionPoliciesValueProvider(Profile* profile);
  ~ExtensionPoliciesValueProvider() override;

  // PolicyValueProvider overrides.
  // Returns each individual extension policy in a dictionary.
  base::Value::Dict GetValues() override;

  base::Value::Dict GetNames() override;

  // extensions::ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;

  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

  // policy::PolicyService::Observer implementation.
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

 private:
  base::Value::Dict GetExtensionPolicyNames(policy::PolicyDomain policy_domain);

  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observation_{this};
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_POLICY_VALUE_PROVIDER_EXTENSION_POLICIES_VALUE_PROVIDER_H_
