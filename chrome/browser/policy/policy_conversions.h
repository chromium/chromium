// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_CONVERSIONS_H_
#define CHROME_BROWSER_POLICY_POLICY_CONVERSIONS_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/browser/ui/webui/localized_string.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace enterprise_management {
class PolicyData;
}

namespace policy {

class PolicyErrorMap;
class Schema;

extern const LocalizedString kPolicySources[POLICY_SOURCE_COUNT];

// A convenience class to retrieve all policies values.
class PolicyConversions {
 public:
  // Maps known policy names to their schema. If a policy is not present, it is
  // not known (either through policy_templates.json or through an extenion's
  // managed storage schema).
  using PolicyToSchemaMap = base::flat_map<std::string, Schema>;

  PolicyConversions();
  virtual ~PolicyConversions();

  // Set to get Chrome and extension policies.
  PolicyConversions& WithBrowserContext(content::BrowserContext* context);
  // Set to get policy types as human friendly string instead of enum integer.
  // Policy types includes policy source, policy scope and policy level.
  // Enabled by default.
  PolicyConversions& EnableConvertTypes(bool enabled);
  // Set to get dictionary policy value as JSON string.
  // Disabled by default.
  PolicyConversions& EnableConvertValues(bool enabled);
  // Set to get device local account policies on ChromeOS.
  // Disabled by default.
  PolicyConversions& EnableDeviceLocalAccountPolicies(bool enabled);
  // Set to get device basic information on ChromeOS.
  // Disabled by default.
  PolicyConversions& EnableDeviceInfo(bool enabled);
  // Set to enable pretty print for all JSON string.
  // Enabled by default.
  PolicyConversions& EnablePrettyPrint(bool enabled);
  // Set to get all user scope policies.
  // Enabled by default.
  PolicyConversions& EnableUserPolicies(bool enabled);

  // Returns the policy data as a base::Value object.
  virtual base::Value ToValue() = 0;

  // Returns the policy data as a JSON string;
  virtual std::string ToJSON();

 protected:
  const Profile* profile() const { return profile_; }

  // Returns policies for Chrome browser.
  virtual base::Value GetChromePolicies();
  // Returns policies for Chrome extensions.
  virtual base::Value GetExtensionsPolicies();
#if defined(OS_CHROMEOS)
  // Returns policies for ChromeOS device.
  virtual base::Value GetDeviceLocalAccountPolicies();
  // Returns device specific information if this device is enterprise managed.
  virtual base::Value GetIdentityFields();
#endif

  std::string ConvertValueToJSON(const base::Value& value);

 private:
  // Returns a copy of |value|. If necessary (which is specified by
  // |convert_values_enabled_|), converts some values to a representation that
  // i18n_template.js will display.
  base::Value CopyAndMaybeConvert(const base::Value& value,
                                  const base::Optional<Schema>& schema);

  // Creates a description of the policy |policy_name| using |policy| and the
  // optional errors in |errors| to determine the status of each policy.
  // |known_policy_schemas| contains |Schema|s for known policies in the same
  // policy namespace of |map|. A policy without an entry in
  // |known_policy_schemas| is an unknown policy.
  base::Value GetPolicyValue(
      const std::string& policy_name,
      const PolicyMap::Entry& policy,
      PolicyErrorMap* errors,
      const base::Optional<PolicyToSchemaMap>& known_policy_schemas);

  // Returns a description of each policy in |map| as Value, using the
  // optional errors in |errors| to determine the status of each policy.
  // |known_policy_schemas| contains |Schema|s for known policies in the same
  // policy namespace of |map|. A policy in |map| but without an entry
  // |known_policy_schemas| is an unknown policy.
  base::Value GetPolicyValues(
      const PolicyMap& map,
      PolicyErrorMap* errors,
      const base::Optional<PolicyToSchemaMap>& known_policy_schemas);

#if defined(OS_CHROMEOS)
  base::Value GetIdentityFieldsFromPolicy(
      const enterprise_management::PolicyData* policy);
#endif

  Profile* profile_;

  bool convert_types_enabled_ = true;
  bool convert_values_enabled_ = false;
  bool device_local_account_policies_enabled_ = false;
  bool device_info_enabled_ = false;
  bool pretty_print_enabled_ = true;
  bool user_policies_enabled_ = true;

  DISALLOW_COPY_AND_ASSIGN(PolicyConversions);
};

class DictionaryPolicyConversions : public PolicyConversions {
 public:
  DictionaryPolicyConversions();
  ~DictionaryPolicyConversions() override;

  base::Value ToValue() override;

 private:
  base::Value GetExtensionsPolicies() override;

#if defined(OS_CHROMEOS)
  base::Value GetDeviceLocalAccountPolicies() override;
#endif

  DISALLOW_COPY_AND_ASSIGN(DictionaryPolicyConversions);
};

class ArrayPolicyConversions : public PolicyConversions {
 public:
  ArrayPolicyConversions();
  ~ArrayPolicyConversions() override;

  base::Value ToValue() override;

 private:
  base::Value GetChromePolicies() override;

  DISALLOW_COPY_AND_ASSIGN(ArrayPolicyConversions);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_CONVERSIONS_H_
