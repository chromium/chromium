// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/scripting_permissions_modifier.h"

#include "chrome/browser/extensions/permissions_updater.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"

namespace extensions {

ScriptingPermissionsModifier::ScriptingPermissionsModifier(
    content::BrowserContext* browser_context,
    const scoped_refptr<const Extension>& extension)
    : browser_context_(browser_context),
      extension_(extension),
      extension_prefs_(ExtensionPrefs::Get(browser_context_)),
      permissions_manager_(PermissionsManager::Get(browser_context_)) {
  DCHECK(extension_);
}

ScriptingPermissionsModifier::~ScriptingPermissionsModifier() = default;

void ScriptingPermissionsModifier::SetWithholdHostPermissions(
    bool should_withhold) {
  DCHECK(permissions_manager_->CanAffectExtension(*extension_));

  if (permissions_manager_->HasWithheldHostPermissions(*extension_) ==
      should_withhold) {
    return;
  }

  // Set the pref first, so that listeners for permission changes get the proper
  // value if they query HasWithheldHostPermissions().
  extension_prefs_->SetWithholdingPermissions(extension_->id(),
                                              should_withhold);

  if (should_withhold)
    WithholdHostPermissions();
  else
    GrantWithheldHostPermissions();
}

void ScriptingPermissionsModifier::GrantHostPermission(const GURL& url) {
  DCHECK(permissions_manager_->CanAffectExtension(*extension_));
  // Check that we don't grant host permission to a restricted URL.
  DCHECK(
      !extension_->permissions_data()->IsRestrictedUrl(url, /*error=*/nullptr))
      << "Cannot grant access to a restricted URL.";

  URLPatternSet explicit_hosts;
  explicit_hosts.AddOrigin(Extension::kValidHostPermissionSchemes, url);
  URLPatternSet scriptable_hosts;
  scriptable_hosts.AddOrigin(UserScript::ValidUserScriptSchemes(), url);

  PermissionsUpdater(browser_context_)
      .GrantRuntimePermissions(
          *extension_,
          PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                        std::move(explicit_hosts), std::move(scriptable_hosts)),
          base::DoNothing());
}

void ScriptingPermissionsModifier::RemoveGrantedHostPermission(
    const GURL& url) {
  DCHECK(permissions_manager_->CanAffectExtension(*extension_));
  DCHECK(permissions_manager_->HasGrantedHostPermission(*extension_, url));

  std::unique_ptr<const PermissionSet> runtime_permissions =
      permissions_manager_->GetRuntimePermissionsFromPrefs(*extension_);

  URLPatternSet explicit_hosts;
  for (const auto& pattern : runtime_permissions->explicit_hosts()) {
    if (pattern.MatchesSecurityOrigin(url))
      explicit_hosts.AddPattern(pattern);
  }
  URLPatternSet scriptable_hosts;
  for (const auto& pattern : runtime_permissions->scriptable_hosts()) {
    if (pattern.MatchesSecurityOrigin(url))
      scriptable_hosts.AddPattern(pattern);
  }

  PermissionsUpdater(browser_context_)
      .RevokeRuntimePermissions(
          *extension_,
          PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                        std::move(explicit_hosts), std::move(scriptable_hosts)),
          base::DoNothing());
}

void ScriptingPermissionsModifier::RemoveBroadGrantedHostPermissions() {
  DCHECK(permissions_manager_->CanAffectExtension(*extension_));

  std::unique_ptr<const PermissionSet> runtime_permissions =
      permissions_manager_->GetRuntimePermissionsFromPrefs(*extension_);

  URLPatternSet explicit_hosts;
  for (const auto& pattern : runtime_permissions->explicit_hosts()) {
    if (pattern.MatchesEffectiveTld()) {
      explicit_hosts.AddPattern(pattern);
    }
  }
  URLPatternSet scriptable_hosts;
  for (const auto& pattern : runtime_permissions->scriptable_hosts()) {
    if (pattern.MatchesEffectiveTld()) {
      scriptable_hosts.AddPattern(pattern);
    }
  }

  PermissionsUpdater(browser_context_)
      .RevokeRuntimePermissions(
          *extension_,
          PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                        std::move(explicit_hosts), std::move(scriptable_hosts)),
          base::DoNothing());
}

void ScriptingPermissionsModifier::RemoveAllGrantedHostPermissions() {
  DCHECK(permissions_manager_->CanAffectExtension(*extension_));
  WithholdHostPermissions();
}

void ScriptingPermissionsModifier::GrantWithheldHostPermissions() {
  const PermissionSet& withheld =
      extension_->permissions_data()->withheld_permissions();

  PermissionSet permissions(APIPermissionSet(), ManifestPermissionSet(),
                            withheld.explicit_hosts().Clone(),
                            withheld.scriptable_hosts().Clone());
  PermissionsUpdater(browser_context_)
      .GrantRuntimePermissions(*extension_, permissions, base::DoNothing());
}

void ScriptingPermissionsModifier::WithholdHostPermissions() {
  std::unique_ptr<const PermissionSet> revokable_permissions =
      permissions_manager_->GetRevokablePermissions(*extension_);
  DCHECK(revokable_permissions);
  PermissionsUpdater(browser_context_)
      .RevokeRuntimePermissions(*extension_, *revokable_permissions,
                                base::DoNothing());
}

}  // namespace extensions
