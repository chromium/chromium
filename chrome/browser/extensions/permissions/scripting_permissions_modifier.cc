// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
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

  if (should_withhold) {
    RemoveAllGrantedHostPermissions();
  } else {
    GrantWithheldHostPermissions();
  }
}

void ScriptingPermissionsModifier::GrantHostPermission(const GURL& url) {
  CHECK(permissions_manager_->CanAffectExtension(*extension_));
  // Check that we don't grant host permission to a restricted URL.
  CHECK(
      !extension_->permissions_data()->IsRestrictedUrl(url, /*error=*/nullptr))
      << "Cannot grant access to a restricted URL.";

  URLPatternSet explicit_hosts;
  explicit_hosts.AddOrigin(Extension::kValidHostPermissionSchemes, url);
  URLPatternSet scriptable_hosts;
  scriptable_hosts.AddOrigin(UserScript::ValidUserScriptSchemes(), url);

  GrantHostPermission(std::move(explicit_hosts), std::move(scriptable_hosts),
                      base::DoNothing());
}

void ScriptingPermissionsModifier::GrantHostPermission(
    const URLPattern& site,
    base::OnceClosure done_callback) {
  CHECK(permissions_manager_->CanAffectExtension(*extension_));

  URLPatternSet explicit_hosts;
  URLPattern explicit_host(site);
  explicit_host.SetValidSchemes(Extension::kValidHostPermissionSchemes);
  explicit_hosts.AddPattern(std::move(explicit_host));

  URLPatternSet scriptable_hosts;
  URLPattern scriptable_host(site);
  scriptable_host.SetValidSchemes(UserScript::ValidUserScriptSchemes());
  scriptable_hosts.AddPattern(std::move(scriptable_host));

  GrantHostPermission(std::move(explicit_hosts), std::move(scriptable_hosts),
                      std::move(done_callback));
}

void ScriptingPermissionsModifier::RemoveGrantedHostPermission(
    const GURL& url) {
  CHECK(permissions_manager_->CanAffectExtension(*extension_));
  CHECK(permissions_manager_->HasGrantedHostPermission(*extension_, url));

  std::unique_ptr<const PermissionSet> runtime_permissions =
      permissions_manager_->GetRuntimePermissionsFromPrefs(*extension_);

  URLPatternSet explicit_hosts;
  for (const auto& pattern : runtime_permissions->explicit_hosts()) {
    if (pattern.MatchesSecurityOrigin(url)) {
      explicit_hosts.AddPattern(pattern);
    }
  }
  URLPatternSet scriptable_hosts;
  for (const auto& pattern : runtime_permissions->scriptable_hosts()) {
    if (pattern.MatchesSecurityOrigin(url)) {
      scriptable_hosts.AddPattern(pattern);
    }
  }

  WithholdHostPermissions(std::move(explicit_hosts),
                          std::move(scriptable_hosts), base::DoNothing());
}

void ScriptingPermissionsModifier::RemoveHostPermissions(
    const URLPattern& pattern,
    base::OnceClosure done_callback) {
  CHECK(permissions_manager_->CanAffectExtension(*extension_));

  // Returns the runtime hosts that overlap the pattern with valid schemes.
  auto get_matching_hosts = [](const URLPattern& pattern, int valid_schemes,
                               const URLPatternSet& runtime_hosts) {
    URLPattern host(pattern);
    host.SetValidSchemes(valid_schemes);

    URLPatternSet matching_hosts;
    for (const auto& runtime_host : runtime_hosts) {
      if (host.OverlapsWith(runtime_host)) {
        matching_hosts.AddPattern(runtime_host);
      }
    }
    return matching_hosts;
  };

  // Revoke all sites which have some intersection with `pattern` from the
  // extension's set of runtime granted host permissions.
  std::unique_ptr<const PermissionSet> runtime_permissions =
      permissions_manager_->GetRuntimePermissionsFromPrefs(*extension_);
  URLPatternSet explicit_hosts =
      get_matching_hosts(pattern, Extension::kValidHostPermissionSchemes,
                         runtime_permissions->explicit_hosts());
  URLPatternSet scriptable_hosts =
      get_matching_hosts(pattern, UserScript::ValidUserScriptSchemes(),
                         runtime_permissions->scriptable_hosts());

  WithholdHostPermissions(std::move(explicit_hosts),
                          std::move(scriptable_hosts),
                          std::move(done_callback));
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

  std::unique_ptr<const PermissionSet> revokable_permissions =
      permissions_manager_->GetRevokablePermissions(*extension_);
  DCHECK(revokable_permissions);
  PermissionsUpdater(browser_context_)
      .RevokeRuntimePermissions(*extension_, *revokable_permissions,
                                base::DoNothing());
}

void ScriptingPermissionsModifier::GrantHostPermission(
    URLPatternSet explicit_hosts,
    URLPatternSet scriptable_hosts,
    base::OnceClosure done_callback) {
  PermissionsUpdater(browser_context_)
      .GrantRuntimePermissions(
          *extension_,
          PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                        std::move(explicit_hosts), std::move(scriptable_hosts)),
          std::move(done_callback));
}

void ScriptingPermissionsModifier::GrantWithheldHostPermissions() {
  const PermissionSet& withheld =
      extension_->permissions_data()->withheld_permissions();

  GrantHostPermission(withheld.explicit_hosts().Clone(),
                      withheld.scriptable_hosts().Clone(), base::DoNothing());
}

void ScriptingPermissionsModifier::WithholdHostPermissions(
    URLPatternSet explicit_hosts,
    URLPatternSet scriptable_hosts,
    base::OnceClosure done_callback) {
  std::unique_ptr<const PermissionSet> permissions_to_remove =
      PermissionSet::CreateIntersection(
          PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                        std::move(explicit_hosts), std::move(scriptable_hosts)),
          *permissions_manager_->GetRevokablePermissions(*extension_),
          URLPatternSet::IntersectionBehavior::kDetailed);
  if (permissions_to_remove->IsEmpty()) {
    std::move(done_callback).Run();
    return;
  }

  PermissionsUpdater(browser_context_)
      .RevokeRuntimePermissions(*extension_, *permissions_to_remove,
                                std::move(done_callback));
}

}  // namespace extensions
