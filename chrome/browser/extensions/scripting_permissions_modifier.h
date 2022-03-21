// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SCRIPTING_PERMISSIONS_MODIFIER_H_
#define CHROME_BROWSER_EXTENSIONS_SCRIPTING_PERMISSIONS_MODIFIER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"

class GURL;

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
class ExtensionPrefs;
class PermissionSet;

// Responsible for managing the majority of click-to-script features, including
// granting, withholding, and querying host permissions, and determining if an
// extension has been affected by the click-to-script project.
class ScriptingPermissionsModifier {
 public:
  ScriptingPermissionsModifier(content::BrowserContext* browser_context,
                               const scoped_refptr<const Extension>& extension);

  ScriptingPermissionsModifier(const ScriptingPermissionsModifier&) = delete;
  ScriptingPermissionsModifier& operator=(const ScriptingPermissionsModifier&) =
      delete;

  ~ScriptingPermissionsModifier();

  // Sets whether Chrome should withhold host permissions from the extension.
  // This may only be called for extensions that can be affected (i.e., for
  // which CanAffectExtension() returns true). Anything else will DCHECK.
  void SetWithholdHostPermissions(bool withhold);

  // Returns whether Chrome has withheld host permissions from the extension.
  // This may only be called for extensions that can be affected (i.e., for
  // which CanAffectExtension() returns true). Anything else will DCHECK.
  // TODO(emiliapaz): Prefer using
  // `PermissionsManager::HasWithheldHostPermissions(extension)`. Remove after
  // all callers are migrated.
  bool HasWithheldHostPermissions() const;

  // Returns true if the associated extension can be affected by
  // runtime host permissions.
  bool CanAffectExtension() const;

  // Grants the extension permission to run on the origin of |url|.
  // This may only be called for extensions that can be affected (i.e., for
  // which CanAffectExtension() returns true). Anything else will DCHECK.
  void GrantHostPermission(const GURL& url);

  // Returns true if the extension has been explicitly granted permission to run
  // on the origin of |url|. This will return true if any permission includes
  // access to the origin of |url|, even if the permission includes others
  // (such as *://*.com/*) or is restricted to a path (that is, an extension
  // with permission for https://google.com/maps will return true for
  // https://google.com). Note: This checks any runtime-granted permissions,
  // which includes both granted optional permissions and permissions granted
  // through the runtime host permissions feature.
  // This may only be called for extensions that can be affected (i.e., for
  // which CanAffectExtension() returns true). Anything else will DCHECK.
  bool HasGrantedHostPermission(const GURL& url) const;

  // Returns true if the extension has runtime granted permission patterns that
  // are sufficiently broad enough to be functionally similar to all sites
  // access.
  bool HasBroadGrantedHostPermissions();

  // Revokes permission to run on the origin of |url|, including any permissions
  // that match or overlap with the origin. For instance, removing access to
  // https://google.com will remove access to *://*.com/* as well.
  // DCHECKs if |url| has not been granted.
  // This may only be called for extensions that can be affected (i.e., for
  // which CanAffectExtension() returns true). Anything else will DCHECK.
  void RemoveGrantedHostPermission(const GURL& url);

  // Revokes host permission patterns granted to the extension that effectively
  // grant access to all urls.
  void RemoveBroadGrantedHostPermissions();

  // Revokes all host permissions granted to the extension. Note that this will
  // only withhold hosts explicitly granted to the extension; this will not
  // implicitly change the value of HasWithheldHostPermissions().
  // This may only be called for extensions that can be affected (i.e., for
  // which CanAffectExtension() returns true). Anything else will DCHECK.
  void RemoveAllGrantedHostPermissions();

  // Takes in a set of permissions and withholds any permissions that should not
  // be granted for the given |extension|, returning a permission set with all
  // of the permissions that can be granted.
  // Note: we pass in |permissions| explicitly here, as this is used during
  // permission initialization, where the active permissions on the extension
  // may not be the permissions to compare against.
  std::unique_ptr<const PermissionSet> WithholdPermissionsIfNecessary(
      const PermissionSet& permissions);

  // Returns the subset of active permissions which can be withheld.
  std::unique_ptr<const PermissionSet> GetRevokablePermissions() const;

  // TODO(emiliapaz): Prefer using
  // `PermissionsManager::GetRuntimePermissionFromPrefs(extension)`. Remove
  // after all callers are migrated. Returns the effective list of
  // runtime-granted permissions for a given `extension` from its prefs.
  std::unique_ptr<const PermissionSet> GetRuntimePermissionsFromPrefs() const;

 private:
  // Grants any withheld host permissions.
  void GrantWithheldHostPermissions();

  // Revokes any granted host permissions.
  void WithholdHostPermissions();

  raw_ptr<content::BrowserContext> browser_context_;

  scoped_refptr<const Extension> extension_;

  raw_ptr<ExtensionPrefs> extension_prefs_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SCRIPTING_PERMISSIONS_MODIFIER_H_
