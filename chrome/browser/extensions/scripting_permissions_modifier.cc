// Copyright 2015 The Chromium Authors. All rights reserved.
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
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"

namespace extensions {

namespace {

// Iterates over |requested_permissions| and returns a permission set of any
// permissions that should be  granted. These include any non-host
// permissions or host permissions that are present in
// |runtime_granted_permissions|. The returned permission set may contain new
// patterns not found in either |requested_permissions| or
// |runtime_granted_permissions| in the case of overlapping host permissions
// (such as *://*.google.com/* and https://*/*, which would intersect with
// https://*.google.com/*).
std::unique_ptr<const PermissionSet> PartitionHostPermissions(
    const PermissionSet& requested_permissions,
    const PermissionSet& runtime_granted_permissions) {
  auto segregate_url_permissions =
      [](const URLPatternSet& requested_patterns,
         const URLPatternSet& runtime_granted_patterns,
         URLPatternSet* granted) {
        *granted = URLPatternSet::CreateIntersection(
            requested_patterns, runtime_granted_patterns,
            URLPatternSet::IntersectionBehavior::kDetailed);
        for (const URLPattern& pattern : requested_patterns) {
          // The chrome://favicon permission is special. It is requested by
          // extensions to access stored favicons, but is not a traditional
          // host permission. Since it cannot be reasonably runtime-granted
          // while the user is on the site (i.e., the user never visits
          // chrome://favicon/), we auto-grant it and treat it like an API
          // permission.
          bool is_chrome_favicon =
              pattern.scheme() == content::kChromeUIScheme &&
              pattern.host() == chrome::kChromeUIFaviconHost;
          if (is_chrome_favicon)
            granted->AddPattern(pattern);
        }
      };

  URLPatternSet granted_explicit_hosts;
  URLPatternSet granted_scriptable_hosts;
  segregate_url_permissions(requested_permissions.explicit_hosts(),
                            runtime_granted_permissions.explicit_hosts(),
                            &granted_explicit_hosts);
  segregate_url_permissions(requested_permissions.scriptable_hosts(),
                            runtime_granted_permissions.scriptable_hosts(),
                            &granted_scriptable_hosts);

  return std::make_unique<PermissionSet>(
      requested_permissions.apis().Clone(),
      requested_permissions.manifest_permissions().Clone(),
      std::move(granted_explicit_hosts), std::move(granted_scriptable_hosts));
}

// Returns true if the extension should even be considered for being affected
// by the runtime host permissions experiment.
bool ShouldConsiderExtension(const Extension& extension) {
  // Certain extensions are always exempt from having permissions withheld.
  if (!util::CanWithholdPermissionsFromExtension(extension))
    return false;

  return true;
}

// Retrieves the effective list of runtime-granted permissions for a given
// |extension| from the |prefs|. ExtensionPrefs doesn't store the valid schemes
// for URLPatterns, which results in the chrome:-scheme being included for
// <all_urls> when retrieving it directly from the prefs; this then causes
// CHECKs to fail when validating that permissions being revoked are present
// (see https://crbug.com/930062).
// Returns null if there are no stored runtime-granted permissions.
// TODO(https://crbug.com/931881): ExtensionPrefs should return properly-bounded
// permissions.
std::unique_ptr<const PermissionSet> GetRuntimePermissionsFromPrefs(
    const Extension& extension,
    const ExtensionPrefs& prefs) {
  std::unique_ptr<const PermissionSet> permissions =
      prefs.GetRuntimeGrantedPermissions(extension.id());

  // If there are no stored permissions, there's nothing to adjust.
  if (!permissions)
    return nullptr;

  // If the extension is allowed to run on chrome:// URLs, then we don't have
  // to adjust anything.
  if (PermissionsData::AllUrlsIncludesChromeUrls(extension.id()))
    return permissions;

  // We need to adjust a pattern if it matches all URLs and includes the
  // chrome:-scheme. These patterns would otherwise match hosts like
  // chrome://settings, which should not be allowed.
  // NOTE: We don't need to adjust for the file scheme, because
  // ExtensionPrefs properly does that based on the extension's file access.
  auto needs_chrome_scheme_adjustment = [](const URLPattern& pattern) {
    return pattern.match_all_urls() &&
           ((pattern.valid_schemes() & URLPattern::SCHEME_CHROMEUI) != 0);
  };

  // NOTE: We don't need to check scriptable_hosts, because the default
  // scriptable_hosts scheme mask omits the chrome:-scheme in normal
  // circumstances (whereas the default explicit scheme does not, in order to
  // allow for patterns like chrome://favicon).

  bool needs_adjustment = std::any_of(permissions->explicit_hosts().begin(),
                                      permissions->explicit_hosts().end(),
                                      needs_chrome_scheme_adjustment);
  // If no patterns need adjustment, return the original set.
  if (!needs_adjustment)
    return permissions;

  // Otherwise, iterate over the explicit hosts, and modify any that need to be
  // tweaked, adding back in permitted chrome:-scheme hosts. This logic mirrors
  // that in PermissionsParser, and is also similar to logic in
  // permissions_api_helpers::UnpackOriginPermissions(), and has some overlap
  // to URLPatternSet::Populate().
  // TODO(devlin): ^^ Ouch. Refactor so that this isn't duplicated.
  URLPatternSet new_explicit_hosts;
  for (const auto& pattern : permissions->explicit_hosts()) {
    if (!needs_chrome_scheme_adjustment(pattern)) {
      new_explicit_hosts.AddPattern(pattern);
      continue;
    }

    URLPattern new_pattern(pattern);
    int new_valid_schemes =
        pattern.valid_schemes() & ~URLPattern::SCHEME_CHROMEUI;
    new_pattern.SetValidSchemes(new_valid_schemes);
    new_explicit_hosts.AddPattern(std::move(new_pattern));
  }

  return std::make_unique<PermissionSet>(
      permissions->apis().Clone(), permissions->manifest_permissions().Clone(),
      std::move(new_explicit_hosts), permissions->scriptable_hosts().Clone());
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

  return extension_prefs_->GetWithholdingPermissions(extension_->id());
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

ScriptingPermissionsModifier::SiteAccess
ScriptingPermissionsModifier::GetSiteAccess(const GURL& url) const {
  SiteAccess access;
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context_);

  // Awkward holder object because permission sets are immutable, and when
  // return from prefs, ownership is passed.
  std::unique_ptr<const PermissionSet> permission_holder;

  const PermissionSet* granted_permissions = nullptr;
  if (!HasWithheldHostPermissions()) {
    // If the extension doesn't have any withheld permissions, we look at the
    // current active permissions.
    // TODO(devlin): This is clunky. It would be nice to have runtime-granted
    // permissions be correctly populated in all cases, rather than looking at
    // two different sets.
    // TODO(devlin): This won't account for granted permissions that aren't
    // currently active, even though the extension may re-request them (and be
    // silently granted them) at any time.
    granted_permissions = &extension_->permissions_data()->active_permissions();
  } else {
    permission_holder = GetRuntimePermissionsFromPrefs(*extension_, *prefs);
    granted_permissions = permission_holder.get();
  }

  DCHECK(granted_permissions);

  const bool is_restricted_site =
      extension_->permissions_data()->IsRestrictedUrl(url, /*error=*/nullptr);

  // For indicating whether an extension has access to a site, we look at the
  // granted permissions, which could include patterns that weren't explicitly
  // requested. However, we should still indicate they are granted, so that the
  // user can revoke them (and because if the extension does request them and
  // they are already granted, they are silently added).
  // The extension should never have access to restricted sites (even if a
  // pattern matches, as it may for e.g. the webstore).
  if (!is_restricted_site &&
      granted_permissions->effective_hosts().MatchesSecurityOrigin(url)) {
    access.has_site_access = true;
  }

  const PermissionSet& withheld_permissions =
      extension_->permissions_data()->withheld_permissions();

  // Be sure to check |access.has_site_access| in addition to withheld
  // permissions, so that we don't indicate we've withheld permission if an
  // extension is granted https://a.com/*, but has *://*/* withheld.
  // We similarly don't show access as withheld for restricted sites, since
  // withheld permissions should only include those that are conceivably
  // grantable.
  if (!is_restricted_site && !access.has_site_access &&
      withheld_permissions.effective_hosts().MatchesSecurityOrigin(url)) {
    access.withheld_site_access = true;
  }

  constexpr bool include_api_permissions = false;
  if (granted_permissions->ShouldWarnAllHosts(include_api_permissions))
    access.has_all_sites_access = true;

  if (withheld_permissions.ShouldWarnAllHosts(include_api_permissions) &&
      !access.has_all_sites_access) {
    access.withheld_all_sites_access = true;
  }

  return access;
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
          base::DoNothing::Once());
}

bool ScriptingPermissionsModifier::HasGrantedHostPermission(
    const GURL& url) const {
  DCHECK(CanAffectExtension());

  return GetRuntimePermissionsFromPrefs(*extension_, *extension_prefs_)
      ->effective_hosts()
      .MatchesSecurityOrigin(url);
}

bool ScriptingPermissionsModifier::HasBroadGrantedHostPermissions() {
  std::unique_ptr<const PermissionSet> runtime_permissions =
      GetRuntimePermissionsFromPrefs(*extension_, *extension_prefs_);

  // Don't consider API permissions in this case.
  constexpr bool kIncludeApiPermissions = false;
  return runtime_permissions->ShouldWarnAllHosts(kIncludeApiPermissions);
}

void ScriptingPermissionsModifier::RemoveGrantedHostPermission(
    const GURL& url) {
  DCHECK(CanAffectExtension());
  DCHECK(HasGrantedHostPermission(url));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context_);
  std::unique_ptr<const PermissionSet> runtime_permissions =
      GetRuntimePermissionsFromPrefs(*extension_, *prefs);

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
          base::DoNothing::Once());
}

void ScriptingPermissionsModifier::RemoveBroadGrantedHostPermissions() {
  DCHECK(CanAffectExtension());

  std::unique_ptr<const PermissionSet> runtime_permissions =
      GetRuntimePermissionsFromPrefs(*extension_, *extension_prefs_);

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
          base::DoNothing::Once());
}

void ScriptingPermissionsModifier::RemoveAllGrantedHostPermissions() {
  DCHECK(CanAffectExtension());
  WithholdHostPermissions();
}

// static
std::unique_ptr<const PermissionSet>
ScriptingPermissionsModifier::WithholdPermissionsIfNecessary(
    const Extension& extension,
    const ExtensionPrefs& extension_prefs,
    const PermissionSet& permissions) {
  if (!ShouldConsiderExtension(extension)) {
    // The withhold creation flag should never have been set in cases where
    // withholding isn't allowed.
    DCHECK(!(extension.creation_flags() & Extension::WITHHOLD_PERMISSIONS));
    return permissions.Clone();
  }

  if (permissions.effective_hosts().is_empty())
    return permissions.Clone();  // No hosts to withhold.

  bool should_withhold = false;
  if (extension.creation_flags() & Extension::WITHHOLD_PERMISSIONS) {
    should_withhold = true;
  } else {
    should_withhold = extension_prefs.GetWithholdingPermissions(extension.id());
  }

  if (!should_withhold)
    return permissions.Clone();

  // Only grant host permissions that the user has explicitly granted at
  // runtime through the runtime host permissions feature or the optional
  // permissions API.
  std::unique_ptr<const PermissionSet> runtime_granted_permissions =
      GetRuntimePermissionsFromPrefs(extension, extension_prefs);
  // If there were no runtime granted permissions found in the prefs, default to
  // a new empty set.
  if (!runtime_granted_permissions) {
    runtime_granted_permissions = std::make_unique<PermissionSet>();
  }
  return PartitionHostPermissions(permissions, *runtime_granted_permissions);
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
      GetRuntimePermissionsFromPrefs(*extension_, *extension_prefs_);
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

  // Revokable permissions are those that would be withheld if there were no
  // runtime-granted permissions.
  PermissionSet empty_runtime_granted_permissions;
  std::unique_ptr<const PermissionSet> granted_permissions =
      PartitionHostPermissions(*current_granted_permissions,
                               empty_runtime_granted_permissions);
  return PermissionSet::CreateDifference(*current_granted_permissions,
                                         *granted_permissions);
}

void ScriptingPermissionsModifier::GrantWithheldHostPermissions() {
  const PermissionSet& withheld =
      extension_->permissions_data()->withheld_permissions();

  PermissionSet permissions(APIPermissionSet(), ManifestPermissionSet(),
                            withheld.explicit_hosts().Clone(),
                            withheld.scriptable_hosts().Clone());
  PermissionsUpdater(browser_context_)
      .GrantRuntimePermissions(*extension_, permissions,
                               base::DoNothing::Once());
}

void ScriptingPermissionsModifier::WithholdHostPermissions() {
  PermissionsUpdater(browser_context_)
      .RevokeRuntimePermissions(*extension_, *GetRevokablePermissions(),
                                base::DoNothing::Once());
}

}  // namespace extensions
