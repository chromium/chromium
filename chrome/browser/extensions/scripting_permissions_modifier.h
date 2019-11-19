// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SCRIPTING_PERMISSIONS_MODIFIER_H_
#define CHROME_BROWSER_EXTENSIONS_SCRIPTING_PERMISSIONS_MODIFIER_H_

#include <memory>
#include <string>

#include "base/macros.h"
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
  struct SiteAccess {
    // The extension has access to the current domain.
    bool has_site_access = false;
    // The extension requested access to the current domain, but it was
    // withheld.
    bool withheld_site_access = false;
    // The extension has access to all sites (or a pattern sufficiently broad
    // as to be functionally similar, such as https://*.com/*). Note that since
    // this includes "broad" patterns, this may be true even if
    // |has_site_access| is false.
    bool has_all_sites_access = false;
    // The extension wants access to all sites (or a pattern sufficiently broad
    // as to be functionally similar, such as https://*.com/*). Note that since
    // this includes "broad" patterns, this may be true even if
    // |withheld_site_access| is false.
    bool withheld_all_sites_access = false;
  };

  ScriptingPermissionsModifier(content::BrowserContext* browser_context,
                               const scoped_refptr<const Extension>& extension);
  ~ScriptingPermissionsModifier();

  // Sets whether Chrome should withhold host permissions from the extension.
  // This may only be called for extensions that can be affected (i.e., for
  // which CanAffectExtension() returns true). Anything else will DCHECK.
  void SetWithholdHostPermissions(bool withhold);

  // Returns whether Chrome has withheld host permissions from the extension.
  // This may only be called for extensions that can be affected (i.e., for
  // which CanAffectExtension() returns true). Anything else will DCHECK.
  bool HasWithheldHostPermissions() const;

  // Returns true if the associated extension can be affected by
  // runtime host permissions.
  bool CanAffectExtension() const;

  // Returns the current access level for the extension on the specified |url|.
  SiteAccess GetSiteAccess(const GURL& url) const;

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

  // Revokes permission to run on the origin of |url|, including any permissions
  // that match or overlap with the origin. For instance, removing access to
  // https://google.com will remove access to *://*.com/* as well.
  // DCHECKs if |url| has not been granted.
  // This may only be called for extensions that can be affected (i.e., for
  // which CanAffectExtension() returns true). Anything else will DCHECK.
  void RemoveGrantedHostPermission(const GURL& url);

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
  static std::unique_ptr<const PermissionSet> WithholdPermissionsIfNecessary(
      const Extension& extension,
      const ExtensionPrefs& extension_prefs,
      const PermissionSet& permissions);

  // Returns the subset of active permissions which can be withheld.
  std::unique_ptr<const PermissionSet> GetRevokablePermissions() const;

 private:
  // Grants any withheld host permissions.
  void GrantWithheldHostPermissions();

  // Revokes any granted host permissions.
  void WithholdHostPermissions();

  content::BrowserContext* browser_context_;

  scoped_refptr<const Extension> extension_;

  ExtensionPrefs* extension_prefs_;

  DISALLOW_COPY_AND_ASSIGN(ScriptingPermissionsModifier);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SCRIPTING_PERMISSIONS_MODIFIER_H_
