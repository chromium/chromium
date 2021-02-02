// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/extensions_permissions_tracker.h"

#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/device_local_account_util.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/manifest_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

ExtensionsPermissionsTracker::ExtensionsPermissionsTracker(
    ExtensionRegistry* registry,
    content::BrowserContext* browser_context)
    : registry_(registry),
      pref_service_(Profile::FromBrowserContext(browser_context)->GetPrefs()) {
  observer_.Add(registry_);
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      pref_names::kInstallForceList,
      base::BindRepeating(
          &ExtensionsPermissionsTracker::OnForcedExtensionsPrefChanged,
          base::Unretained(
              this)));  // Safe as ExtensionsPermissionsTracker
                        // owns pref_change_registrar_ & outlives it
  // Try to load list now.
  OnForcedExtensionsPrefChanged();
}

ExtensionsPermissionsTracker::~ExtensionsPermissionsTracker() = default;

void ExtensionsPermissionsTracker::OnForcedExtensionsPrefChanged() {
  // TODO(crbug.com/1015378): handle pref_names::kExtensionManagement with
  // installation_mode: forced.
  const base::Value* value = pref_service_->Get(pref_names::kInstallForceList);
  if (!value || value->type() != base::Value::Type::DICTIONARY) {
    return;
  }

  extension_safety_ratings_.clear();
  pending_forced_extensions_.clear();

  for (const auto& entry : value->DictItems()) {
    const ExtensionId& extension_id = entry.first;
    // By default the extension permissions are assumed to trigger full warning
    // (false). When the extension is loaded, if all of its permissions is safe,
    // it'll be marked safe (true)
    extension_safety_ratings_.insert(make_pair(extension_id, false));
    const Extension* extension =
        registry_->GetExtensionById(extension_id, ExtensionRegistry::ENABLED);
    if (extension)
      ParseExtensionPermissions(extension);
    else
      pending_forced_extensions_.insert(extension_id);
  }
  if (pending_forced_extensions_.empty())
    UpdateLocalState();
}

bool ExtensionsPermissionsTracker::IsSafePerms(
    const PermissionsData* perms_data) const {
  const PermissionSet& active_permissions = perms_data->active_permissions();
  const APIPermissionSet& api_permissions = active_permissions.apis();
  for (auto* permission : api_permissions) {
    if (permission->info()->requires_managed_session_full_login_warning()) {
      return false;
    }
  }
  const ManifestPermissionSet& manifest_permissions =
      active_permissions.manifest_permissions();
  for (const auto* permission : manifest_permissions) {
    if (permission->RequiresManagedSessionFullLoginWarning()) {
      return false;
    }
  }
  if (active_permissions.ShouldWarnAllHosts() ||
      !active_permissions.effective_hosts().is_empty()) {
    return false;
  }

  return true;
}

void ExtensionsPermissionsTracker::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  auto itr = extension_safety_ratings_.find(extension->id());
  if (itr == extension_safety_ratings_.end())
    return;
  pending_forced_extensions_.erase(extension->id());

  ParseExtensionPermissions(extension);

  // If the extension isn't safe or all extensions are loaded, update the local
  // state.
  if (!itr->second || pending_forced_extensions_.empty())
    UpdateLocalState();
}

void ExtensionsPermissionsTracker::UpdateLocalState() {
  bool any_unsafe = std::any_of(
      extension_safety_ratings_.begin(), extension_safety_ratings_.end(),
      [](const auto& key_value) { return !key_value.second; });

  DCHECK(pending_forced_extensions_.empty() || any_unsafe);

  g_browser_process->local_state()->SetBoolean(
      prefs::kManagedSessionUseFullLoginWarning, any_unsafe);
}

// static
void ExtensionsPermissionsTracker::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kManagedSessionUseFullLoginWarning,
                                true);
}

void ExtensionsPermissionsTracker::ParseExtensionPermissions(
    const Extension* extension) {
  bool is_safe = IsWhitelistedForPublicSession(extension->id()) ||
                 IsSafePerms(extension->permissions_data());

  extension_safety_ratings_[extension->id()] = is_safe;
}

}  // namespace extensions
