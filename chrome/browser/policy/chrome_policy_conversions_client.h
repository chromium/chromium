// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CHROME_POLICY_CONVERSIONS_CLIENT_H_
#define CHROME_BROWSER_POLICY_CHROME_POLICY_CONVERSIONS_CLIENT_H_

#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/browser/policy_conversions_client.h"
#include "components/policy/resources/webui/mojom/policy.mojom-forward.h"
#include "extensions/buildflags/buildflags.h"

namespace content {
class BrowserContext;
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
namespace extensions {
class Extension;
}
#endif

class Profile;

namespace policy {

class PolicyMap;

// ChromePolicyConversionsClient provides an implementation of the
// PolicyConversionsClient interface that is based on Profile and is suitable
// for use in //chrome.
class ChromePolicyConversionsClient : public PolicyConversionsClient {
 public:
  // Creates a ChromePolicyConversionsClient which retrieves Profile-specific
  // policy information from the given |context|.
  explicit ChromePolicyConversionsClient(content::BrowserContext* context);

  ChromePolicyConversionsClient(const ChromePolicyConversionsClient&) = delete;
  ChromePolicyConversionsClient& operator=(
      const ChromePolicyConversionsClient&) = delete;
  ~ChromePolicyConversionsClient() override;

  // PolicyConversionsClient.
  PolicyService* GetPolicyService() const override;
  SchemaRegistry* GetPolicySchemaRegistry() const override;
  const ConfigurationPolicyHandlerList* GetHandlerList() const override;
  bool HasUserPolicies() const override;
  base::ListValue GetExtensionPolicies(PolicyDomain policy_domain) override;
#if BUILDFLAG(IS_CHROMEOS)
  base::ListValue GetDeviceLocalAccountPolicies() override;
  base::DictValue GetIdentityFields() override;
#endif

  // Mojo equivalent of `GetExtensionPolicies()`. Returns one `PolicyGroup` per
  // installed enterprise extension in `policy_domain`, keyed by extension ID.
  // See `GetPolicyMojo()` for how policy values differ from the `base::Value`
  // version.
  base::flat_map<std::string, mojom::PolicyGroupPtr> GetExtensionPoliciesMojo(
      PolicyDomain policy_domain);

 private:
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // Invokes `callback` for each installed extension in `policy_domain` that
  // declares a managed storage schema, passing the extension, its current
  // policies and the schemas of its known policies.
  void ForEachExtensionWithPolicy(
      PolicyDomain policy_domain,
      base::FunctionRef<
          void(const extensions::Extension&,
               const PolicyMap&,
               const std::optional<PolicyConversions::PolicyToSchemaMap>&)>
          callback);
#endif

  raw_ptr<Profile> profile_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CHROME_POLICY_CONVERSIONS_CLIENT_H_
