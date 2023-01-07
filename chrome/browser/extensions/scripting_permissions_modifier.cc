// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/scripting_permissions_modifier.h"

#include "base/callback_helpers.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/url_constants.h"
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

namespace {

// Returns true if the extension should even be considered for being affected
// by the runtime host permissions experiment.
bool ShouldConsiderExtension(const Extension& extension) {
  // Certain extensions are always exempt from having permissions withheld.
  if (!util::CanWithholdPermissionsFromExtension(extension))
    return false;

  return true;
}

}  // namespace

ScriptingPermissionsModifier::ScriptingPermissionsModifier(
    content::BrowserContext* browser_context,
    const scoped_refptr<const Extension>& extension)
    : browser_context_(browser_context),
      extension_(extension),
      extension_prefs_(ExtensionPrefs::Get(browser_context_)) {
  DCHECK(extension_);
}

ScriptingPermissionsModifier::~ScriptingPermissionsModifier() {}

void ScriptingPermissionsModifier::SetWithholdHostPermissions(
    bool should_withhold) {
  DCHECK(CanAffectExtension());

  if (HasWithheldHostPermissions() == should_withhold)
    return;

  // Set the pref first, so that listeners for permission changes get the proper
  // value if they query HasWithheldHostPermissions().
  extension_prefs_->SetWithholdingPermissions(extension_->id(),
                                              should_withhold);

  if (should_withhold)
    WithholdHostPermissions();
  else
    GrantWithheldHostPermissions();
}

bool ScriptingPermissionsModifier::HasWithheldHostPermissions() const {
  DCHECK(CanAffectExtension());

  return PermissionsManager::Get(browser_context_)
      ->HasWithheldHostPermissions(extension_->id());
}

bool ScriptingPermissionsModifier::CanAffectExtension() const {
  if (!ShouldConsiderExtension(*extension_))
    return false;

  // The extension can be affected if it currently has host permissions, or if
  // it did and they are actively withheld.
  return !extension_->permissions_data()
              ->active_permissions()
              .effective_hosts()
              .is_empty() ||
         !extension_->permissions_data()
              ->withheld_permissions()
              .effective_hosts()
              .is_empty();
}

void ScriptingPermissionsModifier::GrantHostPermission(const GURL& url) {
  DCHECK(CanAffectExtension());
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

bool ScriptingPermissionsModifier::HasGrantedHostPermission(
    const GURL& url) const {
  DCHECK(CanAffectExtension());

  return GetRuntimePermissionsFromPrefs()
      ->effective_hosts()
      .MatchesSecurityOrigin(url);
}

bool ScriptingPermissionsModifier::HasBroadGrantedHostPermissions() {
  std::unique_ptr<const PermissionSet> runtime_permissions =
      GetRuntimePermissionsFromPrefs();

  // Don't consider API permissions in this case.
  constexpr bool kIncludeApiPermissions = false;
  return runtime_permissions->ShouldWarnAllHosts(kIncludeApiPermissions);
}

void ScriptingPermissionsModifier::RemoveGrantedHostPermission(
    const GURL& url) {
  DCHECK(CanAffectExtension());
  DCHECK(HasGrantedHostPermission(url));

  std::unique_ptr<const PermissionSet> runtime_permissions =
      GetRuntimePermissionsFromPrefs();

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
  DCHECK(CanAffectExtension());

  std::unique_ptr<const PermissionSet> runtime_permissions =
      GetRuntimePermissionsFromPrefs();

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
  DCHECK(CanAffectExtension());
  WithholdHostPermissions();
}

std::unique_ptr<const PermissionSet>
ScriptingPermissionsModifier::GetRevokablePermissions() const {
  // No extra revokable permissions if the extension couldn't ever be affected.
  if (!ShouldConsiderExtension(*extension_))
    return nullptr;

  // If we aren't withholding host permissions, then there may be some
  // permissions active on the extension that should be revokable. Otherwise,
  // all granted permissions should be stored in the preferences (and these
  // can be a superset of permissions on the extension, as in the case of e.g.
  // granting origins when only a subset is requested by the extension).
  // TODO(devlin): This is confusing and subtle. We should instead perhaps just
  // add all requested hosts as runtime-granted hosts if we aren't withholding
  // host permissions.
  const PermissionSet* current_granted_permissions = nullptr;
  std::unique_ptr<const PermissionSet> runtime_granted_permissions =
      GetRuntimePermissionsFromPrefs();
  std::unique_ptr<const PermissionSet> union_set;
  if (runtime_granted_permissions) {
    union_set = PermissionSet::CreateUnion(
        *runtime_granted_permissions,
        extension_->permissions_data()->active_permissions());
    current_granted_permissions = union_set.get();
  } else {
    current_granted_permissions =
        &extension_->permissions_data()->active_permissions();
  }

  // Unrevokable permissions include granted API permissions, manifest
  // permissions, and host permissions that are always allowed.
  PermissionSet unrevokable_permissions(
      current_granted_permissions->apis().Clone(),
      current_granted_permissions->manifest_permissions().Clone(),
      URLPatternSet(), URLPatternSet());
  {
    // TODO(devlin): We do this pattern of "required + optional" enough. Make it
    // a part of PermissionsParser and stop duplicating the set each time.
    std::unique_ptr<PermissionSet> requested_permissions =
        PermissionSet::CreateUnion(
            PermissionsParser::GetRequiredPermissions(extension_.get()),
            PermissionsParser::GetOptionalPermissions(extension_.get()));
    ExtensionsBrowserClient::Get()->AddAdditionalAllowedHosts(
        *requested_permissions, &unrevokable_permissions);
  }

  // Revokable permissions are, predictably, any in the current set that aren't
  // considered unrevokable.
  return PermissionSet::CreateDifference(*current_granted_permissions,
                                         unrevokable_permissions);
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
      GetRevokablePermissions();
  DCHECK(revokable_permissions);
  PermissionsUpdater(browser_context_)
      .RevokeRuntimePermissions(*extension_, *revokable_permissions,
                                base::DoNothing());
}

std::unique_ptr<const PermissionSet>
ScriptingPermissionsModifier::GetRuntimePermissionsFromPrefs() const {
  return PermissionsManager::Get(browser_context_)
      ->GetRuntimePermissionsFromPrefs(*extension_);
}

}  // namespace extensions
