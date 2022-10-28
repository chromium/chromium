// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_TEST_UTIL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_management_constants.h"
#include "components/policy/core/common/policy_bundle.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_id.h"

namespace policy {
class MockConfigurationPolicyProvider;
class PolicyBundle;
}  // namespace policy

namespace extensions {

// Base class for essential routines on preference manipulation.
class ExtensionManagementPrefUpdaterBase {
 public:
  ExtensionManagementPrefUpdaterBase();

  ExtensionManagementPrefUpdaterBase(
      const ExtensionManagementPrefUpdaterBase&) = delete;
  ExtensionManagementPrefUpdaterBase& operator=(
      const ExtensionManagementPrefUpdaterBase&) = delete;

  virtual ~ExtensionManagementPrefUpdaterBase();

  // Helper functions for per extension settings.
  void UnsetPerExtensionSettings(const ExtensionId& id);
  void ClearPerExtensionSettings(const ExtensionId& id);

  // Helper functions for 'installation_mode' manipulation.
  void SetBlocklistedByDefault(bool value);
  void ClearInstallationModesForIndividualExtensions();
  void SetIndividualExtensionInstallationAllowed(const ExtensionId& id,
                                                 bool allowed);
  void SetIndividualExtensionAutoInstalled(const ExtensionId& id,
                                           const std::string& update_url,
                                           bool forced);

  // Helper functions for 'install_sources' manipulation.
  void UnsetInstallSources();
  void ClearInstallSources();
  void AddInstallSource(const std::string& install_source);
  void RemoveInstallSource(const std::string& install_source);

  // Helper functions for 'allowed_types' manipulation.
  void UnsetAllowedTypes();
  void ClearAllowedTypes();
  void AddAllowedType(const std::string& allowed_type);
  void RemoveAllowedType(const std::string& allowed_type);

  // Helper functions for 'blocked_permissions' manipulation. |prefix| can be
  // kWildCard or a valid extension ID.
  void UnsetBlockedPermissions(const std::string& prefix);
  void ClearBlockedPermissions(const std::string& prefix);
  void AddBlockedPermission(const std::string& prefix,
                            const std::string& permission);
  void RemoveBlockedPermission(const std::string& prefix,
                               const std::string& permission);

  // Helper function for 'blocked_install_message' manipulation.
  // |id| is extension ID.
  void SetBlockedInstallMessage(const ExtensionId& id,
                                const std::string& custom_error);

  // Helper functions for 'runtime_blocked_hosts' manipulation. |prefix| can be
  // kWildCard or a valid extension ID.
  void UnsetPolicyBlockedHosts(const std::string& prefix);
  void ClearPolicyBlockedHosts(const std::string& prefix);
  void AddPolicyBlockedHost(const std::string& prefix, const std::string& host);
  void RemovePolicyBlockedHost(const std::string& prefix,
                               const std::string& host);

  // Helper functions for 'runtime_allowed_hosts' manipulation. |prefix| can be
  // kWildCard or a valid extension ID.
  void UnsetPolicyAllowedHosts(const std::string& prefix);
  void ClearPolicyAllowedHosts(const std::string& prefix);
  void AddPolicyAllowedHost(const std::string& prefix, const std::string& host);
  void RemovePolicyAllowedHost(const std::string& prefix,
                               const std::string& host);

  // Helper functions for 'allowed_permissions' manipulation. |id| must be a
  // valid extension ID.
  void UnsetAllowedPermissions(const std::string& id);
  void ClearAllowedPermissions(const std::string& id);
  void AddAllowedPermission(const std::string& id,
                            const std::string& permission);
  void RemoveAllowedPermission(const std::string& id,
                               const std::string& permission);

  // Helper functions for 'minimum_version_required' manipulation. |id| must be
  // a valid extension ID.
  void SetMinimumVersionRequired(const std::string& id,
                                 const std::string& version);
  void UnsetMinimumVersionRequired(const std::string& id);

  // Expose a read-only preference to user.
  const base::Value::Dict* GetPref();

 protected:
  // Set the preference with |pref|, pass the ownership of it as well.
  // This function must be called before accessing publicly exposed functions,
  // for example in constructor of subclass.
  void SetPref(base::Value::Dict pref);

  // Take the preference. This function must be called after accessing publicly
  // exposed functions, for example in destructor of subclass.
  base::Value::Dict TakePref();

 private:
  // Helper functions for manipulating sub properties like list of strings.
  void ClearList(const std::string& path);
  void AddStringToList(const std::string& path, const std::string& str);
  void RemoveStringFromList(const std::string& path, const std::string& str);

  base::Value::Dict pref_;
};

// A helper class to manipulate the extension management preference in unit
// tests.
template <class TestingPrefService>
class ExtensionManagementPrefUpdater
    : public ExtensionManagementPrefUpdaterBase {
 public:
  explicit ExtensionManagementPrefUpdater(TestingPrefService* service)
      : service_(service) {
    const base::Value* pref_value =
        service_->GetManagedPref(pref_names::kExtensionManagement);
    base::Value::Dict dict;
    if (pref_value && pref_value->is_dict()) {
      dict = pref_value->GetDict().Clone();
    }
    SetPref(std::move(dict));
  }

  ExtensionManagementPrefUpdater(const ExtensionManagementPrefUpdater&) =
      delete;
  ExtensionManagementPrefUpdater& operator=(
      const ExtensionManagementPrefUpdater&) = delete;

  ~ExtensionManagementPrefUpdater() override {
    service_->SetManagedPref(pref_names::kExtensionManagement,
                             base::Value(TakePref()));
  }

 private:
  raw_ptr<TestingPrefService> service_;
};

// A helper class to manipulate the extension management policy in browser
// tests.
class ExtensionManagementPolicyUpdater
    : public ExtensionManagementPrefUpdaterBase {
 public:
  explicit ExtensionManagementPolicyUpdater(
      policy::MockConfigurationPolicyProvider* provider);

  ExtensionManagementPolicyUpdater(const ExtensionManagementPolicyUpdater&) =
      delete;
  ExtensionManagementPolicyUpdater& operator=(
      const ExtensionManagementPolicyUpdater&) = delete;

  ~ExtensionManagementPolicyUpdater() override;

 private:
  raw_ptr<policy::MockConfigurationPolicyProvider> provider_;
  policy::PolicyBundle policies_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_TEST_UTIL_H_
