// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/value_provider/chrome_policies_value_provider.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/value_provider/value_provider_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/core/common/schema_registry.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "components/policy/policy_constants.h"
#endif  // !BUILDFLAG(IS_CHROMEOS)

ChromePoliciesValueProvider::ChromePoliciesValueProvider(Profile* profile)
    : profile_(profile) {
  GetPolicyService(profile_)->AddObserver(policy::POLICY_DOMAIN_CHROME, this);
  policy::SchemaRegistry* registry = profile_->GetOriginalProfile()
                                         ->GetPolicySchemaRegistryService()
                                         ->registry();
  registry->AddObserver(this);
}

ChromePoliciesValueProvider::~ChromePoliciesValueProvider() {
  GetPolicyService(profile_)->RemoveObserver(policy::POLICY_DOMAIN_CHROME,
                                             this);
  policy::SchemaRegistry* registry = profile_->GetOriginalProfile()
                                         ->GetPolicySchemaRegistryService()
                                         ->registry();
  registry->RemoveObserver(this);
}

base::Value::Dict ChromePoliciesValueProvider::GetValues() {
  return policy::PolicyConversions(
             std::make_unique<policy::ChromePolicyConversionsClient>(profile_))
      .UseChromePolicyConversions()
      .ToValueDict();
}

base::Value::Dict ChromePoliciesValueProvider::GetNames() {
  base::Value::Dict names;

  policy::SchemaRegistry* registry = profile_->GetOriginalProfile()
                                         ->GetPolicySchemaRegistryService()
                                         ->registry();
  scoped_refptr<policy::SchemaMap> schema_map = registry->schema_map();

  // Add Chrome policy names.
  base::Value::List chrome_policy_names;
  policy::PolicyNamespace chrome_ns(policy::POLICY_DOMAIN_CHROME, "");
  const policy::Schema* chrome_schema = schema_map->GetSchema(chrome_ns);
  for (auto it = chrome_schema->GetPropertiesIterator(); !it.IsAtEnd();
       it.Advance()) {
    chrome_policy_names.Append(it.key());
  }
  base::Value::Dict chrome_values;
  chrome_values.Set(policy::kNameKey, policy::kChromePoliciesName);
  chrome_values.Set(policy::kPolicyNamesKey, std::move(chrome_policy_names));
  names.Set(policy::kChromePoliciesId, std::move(chrome_values));

#if !BUILDFLAG(IS_CHROMEOS)
  // Add precedence policy names.
  base::Value::List precedence_policy_names;
  for (auto* policy : policy::metapolicy::kPrecedence) {
    precedence_policy_names.Append(policy);
  }
  base::Value::Dict precedence_values;
  precedence_values.Set(policy::kNameKey, policy::kPrecedencePoliciesName);
  precedence_values.Set(policy::kPolicyNamesKey,
                        std::move(precedence_policy_names));
  names.Set(policy::kPrecedencePoliciesId, std::move(precedence_values));
#endif  // !BUILDFLAG(IS_CHROMEOS)
  return names;
}

void ChromePoliciesValueProvider::Refresh() {
  GetPolicyService(profile_)->RefreshPolicies(
      base::BindOnce(&ChromePoliciesValueProvider::OnRefreshPoliciesDone,
                     weak_ptr_factory_.GetWeakPtr()),
      policy::PolicyFetchReason::kUserRequest);
}

void ChromePoliciesValueProvider::OnRefreshPoliciesDone() {
  NotifyValueChange();
}

void ChromePoliciesValueProvider::OnPolicyUpdated(
    const policy::PolicyNamespace& ns,
    const policy::PolicyMap& previous,
    const policy::PolicyMap& current) {
  NotifyValueChange();
}

void ChromePoliciesValueProvider::OnSchemaRegistryUpdated(
    bool has_new_schemas) {
  if (has_new_schemas)
    NotifyValueChange();
}
