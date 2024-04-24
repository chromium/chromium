// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_management_test_util.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "components/crx_file/id_util.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"

namespace extensions {

namespace schema = schema_constants;

namespace {

const char kInstallSourcesPath[] = "*.install_sources";
const char kAllowedTypesPath[] = "*.allowed_types";

std::string make_path(const std::string& a, const std::string& b) {
  return a + "." + b;
}

void RemoveDictionaryPath(base::Value::Dict& dict, std::string_view path) {
  std::string_view current_path(path);
  base::Value::Dict* current_dictionary = &dict;
  size_t delimiter_position = current_path.rfind('.');
  if (delimiter_position != std::string_view::npos) {
    current_dictionary =
        dict.FindDictByDottedPath(current_path.substr(0, delimiter_position));
    if (!current_dictionary)
      return;
    current_path = current_path.substr(delimiter_position + 1);
  }
  current_dictionary->Remove(current_path);
}

}  // namespace

ExtensionManagementPrefUpdaterBase::ExtensionManagementPrefUpdaterBase() {
}

ExtensionManagementPrefUpdaterBase::~ExtensionManagementPrefUpdaterBase() {
  // Make asynchronous calls finished to deliver all preference changes to the
  // NetworkService and extension processes.
  base::RunLoop().RunUntilIdle();
}

// Helper functions for per extension settings ---------------------------------

void ExtensionManagementPrefUpdaterBase::UnsetPerExtensionSettings(
    const ExtensionId& id) {
  DCHECK(crx_file::id_util::IdIsValid(id));
  pref_.Remove(id);
}

void ExtensionManagementPrefUpdaterBase::ClearPerExtensionSettings(
    const ExtensionId& id) {
  DCHECK(crx_file::id_util::IdIsValid(id));
  pref_.Set(id, base::Value::Dict());
}

// Helper functions for 'installation_mode' manipulation -----------------------

void ExtensionManagementPrefUpdaterBase::SetBlocklistedByDefault(bool value) {
  pref_.SetByDottedPath(make_path(schema::kWildcard, schema::kInstallationMode),
                        value ? schema::kBlocked : schema::kAllowed);
}

void ExtensionManagementPrefUpdaterBase::
    ClearInstallationModesForIndividualExtensions() {
  for (auto it : pref_) {
    DCHECK(it.second.is_dict());
    if (it.first != schema::kWildcard) {
      DCHECK(crx_file::id_util::IdIsValid(it.first));
      RemoveDictionaryPath(pref_,
                           make_path(it.first, schema::kInstallationMode));
      RemoveDictionaryPath(pref_, make_path(it.first, schema::kUpdateUrl));
    }
  }
}

void
ExtensionManagementPrefUpdaterBase::SetIndividualExtensionInstallationAllowed(
    const ExtensionId& id,
    bool allowed) {
  DCHECK(crx_file::id_util::IdIsValid(id));
  pref_.SetByDottedPath(make_path(id, schema::kInstallationMode),
                        allowed ? schema::kAllowed : schema::kBlocked);
  RemoveDictionaryPath(pref_, make_path(id, schema::kUpdateUrl));
}

void ExtensionManagementPrefUpdaterBase::SetIndividualExtensionAutoInstalled(
    const ExtensionId& id,
    const std::string& update_url,
    bool forced) {
  DCHECK(crx_file::id_util::IdIsValid(id));
  pref_.SetByDottedPath(
      make_path(id, schema::kInstallationMode),
      forced ? schema::kForceInstalled : schema::kNormalInstalled);
  pref_.SetByDottedPath(make_path(id, schema::kUpdateUrl), update_url);
}

// Helper functions for 'install_sources' manipulation -------------------------

void ExtensionManagementPrefUpdaterBase::UnsetInstallSources() {
  RemoveDictionaryPath(pref_, kInstallSourcesPath);
}

void ExtensionManagementPrefUpdaterBase::ClearInstallSources() {
  ClearList(kInstallSourcesPath);
}

void ExtensionManagementPrefUpdaterBase::AddInstallSource(
    const std::string& install_source) {
  AddStringToList(kInstallSourcesPath, install_source);
}

void ExtensionManagementPrefUpdaterBase::RemoveInstallSource(
    const std::string& install_source) {
  RemoveStringFromList(kInstallSourcesPath, install_source);
}

// Helper functions for 'allowed_types' manipulation ---------------------------

void ExtensionManagementPrefUpdaterBase::UnsetAllowedTypes() {
  RemoveDictionaryPath(pref_, kAllowedTypesPath);
}

void ExtensionManagementPrefUpdaterBase::ClearAllowedTypes() {
  ClearList(kAllowedTypesPath);
}

void ExtensionManagementPrefUpdaterBase::AddAllowedType(
    const std::string& allowed_type) {
  AddStringToList(kAllowedTypesPath, allowed_type);
}

void ExtensionManagementPrefUpdaterBase::RemoveAllowedType(
    const std::string& allowed_type) {
  RemoveStringFromList(kAllowedTypesPath, allowed_type);
}

// Helper functions for 'blocked_permissions' manipulation ---------------------

void ExtensionManagementPrefUpdaterBase::UnsetBlockedPermissions(
    const std::string& prefix) {
  DCHECK(prefix == schema::kWildcard || crx_file::id_util::IdIsValid(prefix));
  RemoveDictionaryPath(pref_, make_path(prefix, schema::kBlockedPermissions));
}

void ExtensionManagementPrefUpdaterBase::ClearBlockedPermissions(
    const std::string& prefix) {
  DCHECK(prefix == schema::kWildcard || crx_file::id_util::IdIsValid(prefix));
  ClearList(make_path(prefix, schema::kBlockedPermissions));
}

void ExtensionManagementPrefUpdaterBase::AddBlockedPermission(
    const std::string& prefix,
    const std::string& permission) {
  DCHECK(prefix == schema::kWildcard || crx_file::id_util::IdIsValid(prefix));
  AddStringToList(make_path(prefix, schema::kBlockedPermissions), permission);
}

void ExtensionManagementPrefUpdaterBase::RemoveBlockedPermission(
    const std::string& prefix,
    const std::string& permission) {
  DCHECK(prefix == schema::kWildcard || crx_file::id_util::IdIsValid(prefix));
  RemoveStringFromList(make_path(prefix, schema::kBlockedPermissions),
                       permission);
}

// Helper function for 'blocked_install_message' manipulation -----------------

void ExtensionManagementPrefUpdaterBase::SetBlockedInstallMessage(
    const ExtensionId& id,
    const std::string& blocked_install_message) {
  DCHECK(id == schema::kWildcard || crx_file::id_util::IdIsValid(id));
  pref_.SetByDottedPath(make_path(id, schema::kBlockedInstallMessage),
                        blocked_install_message);
}

// Helper functions for 'runtime_blocked_hosts' manipulation ------------------

void ExtensionManagementPrefUpdaterBase::UnsetPolicyBlockedHosts(
    const std::string& prefix) {
  DCHECK(prefix == schema::kWildcard || crx_file::id_util::IdIsValid(prefix));
  RemoveDictionaryPath(pref_, make_path(prefix, schema::kPolicyBlockedHosts));
}

void ExtensionManagementPrefUpdaterBase::ClearPolicyBlockedHosts(
    const std::string& prefix) {
  DCHECK(prefix == schema::kWildcard || crx_file::id_util::IdIsValid(prefix));
  ClearList(make_path(prefix, schema::kPolicyBlockedHosts));
}

void ExtensionManagementPrefUpdaterBase::AddPolicyBlockedHost(
    const std::string& prefix,
    const std::string& host) {
  DCHECK(prefix == schema::kWildcard || crx_file::id_util::IdIsValid(prefix));
  AddStringToList(make_path(prefix, schema::kPolicyBlockedHosts), host);
}

void ExtensionManagementPrefUpdaterBase::RemovePolicyBlockedHost(
    const std::string& prefix,
    const std::string& host) {
  DCHECK(prefix == schema::kWildcard || crx_file::id_util::IdIsValid(prefix));
  RemoveStringFromList(make_path(prefix, schema::kPolicyBlockedHosts), host);
}

// Helper functions for 'runtime_allowed_hosts' manipulation ------------------

void ExtensionManagementPrefUpdaterBase::UnsetPolicyAllowedHosts(
    const std::string& prefix) {
  DCHECK(prefix == schema::kWildcard || crx_file::id_util::IdIsValid(prefix));
  RemoveDictionaryPath(pref_, make_path(prefix, schema::kPolicyAllowedHosts));
}

void ExtensionManagementPrefUpdaterBase::ClearPolicyAllowedHosts(
    const std::string& prefix) {
  DCHECK(prefix == schema::kWildcard || crx_file::id_util::IdIsValid(prefix));
  ClearList(make_path(prefix, schema::kPolicyAllowedHosts));
}

void ExtensionManagementPrefUpdaterBase::AddPolicyAllowedHost(
    const std::string& prefix,
    const std::string& host) {
  DCHECK(prefix == schema::kWildcard || crx_file::id_util::IdIsValid(prefix));
  AddStringToList(make_path(prefix, schema::kPolicyAllowedHosts), host);
}

void ExtensionManagementPrefUpdaterBase::RemovePolicyAllowedHost(
    const std::string& prefix,
    const std::string& host) {
  DCHECK(prefix == schema::kWildcard || crx_file::id_util::IdIsValid(prefix));
  RemoveStringFromList(make_path(prefix, schema::kPolicyAllowedHosts), host);
}

// Helper functions for 'allowed_permissions' manipulation ---------------------

void ExtensionManagementPrefUpdaterBase::UnsetAllowedPermissions(
    const std::string& id) {
  DCHECK(crx_file::id_util::IdIsValid(id));
  RemoveDictionaryPath(pref_, make_path(id, schema::kAllowedPermissions));
}

void ExtensionManagementPrefUpdaterBase::ClearAllowedPermissions(
    const std::string& id) {
  DCHECK(crx_file::id_util::IdIsValid(id));
  ClearList(make_path(id, schema::kAllowedPermissions));
}

void ExtensionManagementPrefUpdaterBase::AddAllowedPermission(
    const std::string& id,
    const std::string& permission) {
  DCHECK(crx_file::id_util::IdIsValid(id));
  AddStringToList(make_path(id, schema::kAllowedPermissions), permission);
}

void ExtensionManagementPrefUpdaterBase::RemoveAllowedPermission(
    const std::string& id,
    const std::string& permission) {
  DCHECK(crx_file::id_util::IdIsValid(id));
  RemoveStringFromList(make_path(id, schema::kAllowedPermissions), permission);
}

// Helper functions for 'minimum_version_required' manipulation ----------------

void ExtensionManagementPrefUpdaterBase::SetMinimumVersionRequired(
    const std::string& id,
    const std::string& version) {
  DCHECK(crx_file::id_util::IdIsValid(id));
  pref_.SetByDottedPath(make_path(id, schema::kMinimumVersionRequired),
                        version);
}

void ExtensionManagementPrefUpdaterBase::UnsetMinimumVersionRequired(
    const std::string& id) {
  DCHECK(crx_file::id_util::IdIsValid(id));
  RemoveDictionaryPath(pref_, make_path(id, schema::kMinimumVersionRequired));
}

// Expose a read-only preference to user ---------------------------------------

const base::Value::Dict* ExtensionManagementPrefUpdaterBase::GetPref() {
  return &pref_;
}

// Private section functions ---------------------------------------------------

void ExtensionManagementPrefUpdaterBase::SetPref(base::Value::Dict pref) {
  pref_ = std::move(pref);
}

base::Value::Dict ExtensionManagementPrefUpdaterBase::TakePref() {
  return std::move(pref_);
}

void ExtensionManagementPrefUpdaterBase::ClearList(const std::string& path) {
  pref_.SetByDottedPath(path, base::Value::List());
}

void ExtensionManagementPrefUpdaterBase::AddStringToList(
    const std::string& path,
    const std::string& str) {
  base::Value::List* list_value_weak = pref_.FindListByDottedPath(path);
  if (!list_value_weak) {
    list_value_weak =
        &pref_.SetByDottedPath(path, base::Value::List())->GetList();
  }
  CHECK(!base::Contains(*list_value_weak, base::Value(str)));
  list_value_weak->Append(str);
}

void ExtensionManagementPrefUpdaterBase::RemoveStringFromList(
    const std::string& path,
    const std::string& str) {
  base::Value::List* list_value = pref_.FindListByDottedPath(path);
  if (list_value)
    CHECK_GT(list_value->EraseValue(base::Value(str)), 0u);
}

// ExtensionManagementPolicyUpdater --------------------------------------------

ExtensionManagementPolicyUpdater::ExtensionManagementPolicyUpdater(
    policy::MockConfigurationPolicyProvider* policy_provider)
    : provider_(policy_provider), policies_(provider_->policies().Clone()) {
  const base::Value* policy_value =
      policies_
          .Get(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                       std::string()))
          .GetValue(policy::key::kExtensionSettings, base::Value::Type::DICT);
  base::Value::Dict dict;
  if (policy_value && policy_value->is_dict()) {
    dict = policy_value->GetDict().Clone();
  }
  SetPref(std::move(dict));
}

ExtensionManagementPolicyUpdater::~ExtensionManagementPolicyUpdater() {
  policies_
      .Get(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()))
      .Set(policy::key::kExtensionSettings, policy::POLICY_LEVEL_MANDATORY,
           policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
           base::Value(TakePref()), nullptr);
  provider_->UpdatePolicy(std::move(policies_));
}

}  // namespace extensions
