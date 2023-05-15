// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_VALUE_PROVIDER_CHROME_POLICIES_VALUE_PROVIDER_H_
#define CHROME_BROWSER_POLICY_VALUE_PROVIDER_CHROME_POLICIES_VALUE_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/policy/value_provider/policy_value_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/schema_registry.h"

// Returns the names and values of Chrome policies along with precedence
// policies for non-ChromeOS devices and device local account policies and
// identity fields for ChromeOS devices.
class ChromePoliciesValueProvider : public policy::PolicyValueProvider,
                                    public policy::PolicyService::Observer,
                                    public policy::SchemaRegistry::Observer {
 public:
  explicit ChromePoliciesValueProvider(Profile* profile);
  ~ChromePoliciesValueProvider() override;

  // PolicyValueProvider implementation.
  base::Value::Dict GetValues() override;

  base::Value::Dict GetNames() override;

  void Refresh() override;

  // policy::PolicyService::Observer implementation.
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

  // policy::SchemaRegistry::Observer implementation.
  void OnSchemaRegistryUpdated(bool has_new_schemas) override;

 private:
  void OnRefreshPoliciesDone();

  raw_ptr<Profile> profile_;
  base::WeakPtrFactory<ChromePoliciesValueProvider> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_POLICY_VALUE_PROVIDER_CHROME_POLICIES_VALUE_PROVIDER_H_
