// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_management_test_util.h"

#include <string>
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

void RemoveDictionaryPath(base::Value* dict, base::StringPiece path) {
  CHECK(dict->is_dict());
  base::StringPiece current_path(path);
  base::Value* current_dictionary = dict;
  size_t delimiter_position = current_path.rfind('.');
  if (delimiter_position != base::StringPiece::npos) {
    current_dictionary =
        dict->FindPath(current_path.substr(0, delimiter_position));
    if (!current_dictionary)
      return;
    current_path = current_path.substr(delimiter_position + 1);
  }
  current_dictionary->RemoveKey(current_path);
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
  pref_->RemoveKey(id);
}

void ExtensionManagementPrefUpdaterBase::ClearPerExtensionSettings(
    const ExtensionId& id) {
  DCHECK(crx_file::id_util::IdIsValid(id));
  pref_->SetKey(id, base::DictionaryValue());
}

// Helper functions for 'installation_mode' manipulation -----------------------

void ExtensionManagementPrefUpdaterBase::SetBlocklistedByDefault(bool value) {
  pref_->SetString(make_path(schema::kWildcard, schema::kInstallationMode),
                   value ? schema::kBlocked : schema::kAllowed);
}

void ExtensionManagementPrefUpdaterBase::
    ClearInstallationModesForIndividualExtensions() {
  for (auto it : pref_->DictItems()) {
    DCHECK(it.second.is_dict());
    if (it.first != schema::kWildcard) {
      DCHECK(crx_file::id_util::IdIsValid(it.first));
      RemoveDictionaryPath(pref_.get(),
                           make_path(it.first, schema::kInstallationMode));
      RemoveDictionaryPath(pref_.get(),
                           make_path(it.first, schema::kUpdateUrl));
    }
  }
}

void
ExtensionManagementPrefUpdaterBase::SetIndividualExtensionInstallationAllowed(
    const ExtensionId& id,
    bool allowed) {
  DCHECK(crx_file::id_util::IdIsValid(id));
  pref_->SetString(make_path(id, schema::kInstallationMode),
                   allowed ? schema::kAllowed : schema::kBlocked);
  RemoveDictionaryPath(pref_.get(), make_path(id, schema::kUpdateUrl));
}

void ExtensionManagementPrefUpdaterBase::SetIndividualExtensionAutoInstalled(
    const ExtensionId& id,
    const std::string& update_url,
    bool forced) {
  DCHECK(crx_file::id_util::IdIsValid(id));
  pref_->SetString(make_path(id, schema::kInstallationMode),
                   forced ? schema::kForceInstalled : schema::kNormalInstalled);
  pref_->SetString(make_path(id, schema::kUpdateUrl), update_url);
}

// Helper functions for 'install_sources' manipulation -------------------------

void ExtensionManagementPrefUpdaterBase::UnsetInstallSources() {
  RemoveDictionaryPath(pref_.get(), kInstallSourcesPath);
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
  RemoveDictionaryPath(pref_.get(), kAllowedTypesPath);
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
  RemoveDictionaryPath(pref_.get(),
                       make_path(prefix, schema::kBlockedPermissions));
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
  pref_->SetString(make_path(id, schema::kBlockedInstallMessage),
                   blocked_install_message);
}

// Helper functions for 'runtime_blocked_hosts' manipulation ------------------

void ExtensionManagementPrefUpdaterBase::UnsetPolicyBlockedHosts(
    const std::string& prefix) {
  DCHECK(prefix == schema::kWildcard || crx_file::id_util::IdIsValid(prefix));
  RemoveDictionaryPath(pref_.get(),
                       make_path(prefix, schema::kPolicyBlockedHosts));
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
  RemoveDictionaryPath(pref_.get(),
                       make_path(prefix, schema::kPolicyAllowedHosts));
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
  RemoveDictionaryPath(pref_.get(), make_path(id, schema::kAllowedPermissions));
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
  pref_->SetString(make_path(id, schema::kMinimumVersionRequired), version);
}

void ExtensionManagementPrefUpdaterBase::UnsetMinimumVersionRequired(
    const std::string& id) {
  DCHECK(crx_file::id_util::IdIsValid(id));
  RemoveDictionaryPath(pref_.get(),
                       make_path(id, schema::kMinimumVersionRequired));
}

// Expose a read-only preference to user ---------------------------------------

const base::DictionaryValue* ExtensionManagementPrefUpdaterBase::GetPref() {
  return pref_.get();
}

// Private section functions ---------------------------------------------------

void ExtensionManagementPrefUpdaterBase::SetPref(
    std::unique_ptr<base::DictionaryValue> pref) {
  pref_.reset(pref.release());
}

std::unique_ptr<base::DictionaryValue>
ExtensionManagementPrefUpdaterBase::TakePref() {
  return std::move(pref_);
}

void ExtensionManagementPrefUpdaterBase::ClearList(const std::string& path) {
  pref_->Set(path, std::make_unique<base::ListValue>());
}

void ExtensionManagementPrefUpdaterBase::AddStringToList(
    const std::string& path,
    const std::string& str) {
  base::ListValue* list_value_weak = nullptr;
  if (!pref_->GetList(path, &list_value_weak)) {
    auto list_value = std::make_unique<base::ListValue>();
    list_value_weak = list_value.get();
    pref_->Set(path, std::move(list_value));
  }
  CHECK(
      !base::Contains(list_value_weak->GetListDeprecated(), base::Value(str)));
  list_value_weak->Append(str);
}

void ExtensionManagementPrefUpdaterBase::RemoveStringFromList(
    const std::string& path,
    const std::string& str) {
  base::ListValue* list_value = nullptr;
  if (pref_->GetList(path, &list_value))
    CHECK_GT(list_value->EraseListValue(base::Value(str)), 0u);
}

// ExtensionManagementPolicyUpdater --------------------------------------------

ExtensionManagementPolicyUpdater::ExtensionManagementPolicyUpdater(
    policy::MockConfigurationPolicyProvider* policy_provider)
    : provider_(policy_provider), policies_(new policy::PolicyBundle) {
  policies_->CopyFrom(provider_->policies());
  const base::Value* policy_value =
      policies_
          ->Get(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                        std::string()))
          .GetValue(policy::key::kExtensionSettings, base::Value::Type::DICT);
  std::unique_ptr<base::DictionaryValue> dict_value(new base::DictionaryValue);
  if (policy_value && policy_value->is_dict()) {
    dict_value = base::DictionaryValue::From(
        base::Value::ToUniquePtrValue(policy_value->Clone()));
  }
  SetPref(std::move(dict_value));
}

ExtensionManagementPolicyUpdater::~ExtensionManagementPolicyUpdater() {
  policies_
      ->Get(
          policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()))
      .Set(policy::key::kExtensionSettings, policy::POLICY_LEVEL_MANDATORY,
           policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
           std::move(*TakePref()), nullptr);
  provider_->UpdatePolicy(std::move(policies_));
}

}  // namespace extensions
