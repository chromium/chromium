// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PERMISSIONS_PERMISSIONS_UPDATER_H_
#define CHROME_BROWSER_EXTENSIONS_PERMISSIONS_PERMISSIONS_UPDATER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "extensions/browser/extension_event_histogram_value.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;
class PermissionSet;
class URLPatternSet;

// Updates an Extension's active and granted permissions in persistent storage
// and notifies interested parties of the changes.
class PermissionsUpdater {
 public:
  // If INIT_FLAG_TRANSIENT is specified, this updater is being used for an
  // extension that is not actually installed (and instead is just being
  // initialized e.g. to display the permission warnings in an install prompt).
  // In these cases, this updater should follow all rules below.
  //   a) don't check prefs for stored permissions.
  //   b) don't send notifications of permission changes, because there is no
  //      installed extension that would be affected.
  enum InitFlag {
    INIT_FLAG_NONE = 0,
    INIT_FLAG_TRANSIENT = 1 << 0,
  };

  enum RemoveType {
    // Permissions will be removed from the active set of permissions, but not
    // the stored granted permissions. This allows the extension to re-add the
    // permissions without further prompting.
    REMOVE_SOFT,
    // Permissions will be removed from the active set of permissions and the
    // stored granted permissions. The extension will need to re-prompt the
    // user to re-add the permissions.
    // TODO(devlin): REMOVE_HARD is only exercised in unit tests, but we have
    // the desire to be able to able to surface revoking optional permissions to
    // the user. We should either a) pursue it in earnest or b) remove support
    // (and potentially add it back at a later date).
    REMOVE_HARD,
  };

  explicit PermissionsUpdater(content::BrowserContext* browser_context);
  PermissionsUpdater(content::BrowserContext* browser_context,
                     InitFlag init_flag);

  PermissionsUpdater(const PermissionsUpdater&) = delete;
  PermissionsUpdater& operator=(const PermissionsUpdater&) = delete;

  ~PermissionsUpdater();

  // Grants |permissions| that were defined as optional in the manifest to
  // |extension|, updating the active permission set and notifying any
  // observers. This method assumes the user has already been prompted, if
  // necessary, for the extra permissions.
  // NOTE: This should only be used for granting permissions defined in the
  // extension's optional permissions set through the permissions API.
  void GrantOptionalPermissions(const Extension& extension,
                                const PermissionSet& permissions,
                                base::OnceClosure completion_callback);

  // Grants |permissions| that were withheld at installation and granted at
  // runtime to |extension|, updating the active permission set and notifying
  // any observers. |permissions| may contain permissions that were not
  // explicitly requested by the extension; if this happens, those permissions
  // will be added to the runtime-granted permissions in the preferences, but
  // will not be granted to the extension object or process itself.
  // NOTE: This should only be used for granting permissions through the runtime
  // host permissions feature.
  void GrantRuntimePermissions(const Extension& extension,
                               const PermissionSet& permissions,
                               base::OnceClosure completion_callback);

  // Removes |permissions| that were defined as optional in the manifest from
  // the |extension|, updating the active permission set and notifying any
  // observers. |remove_type| specifies whether the permissions should be
  // revoked from the preferences, thus requiring the extension to re-prompt
  // the user if it wants to add them back.
  // NOTE: This should only be used for removing permissions defined in the
  // extension's optional permissions set through the permissions API.
  void RevokeOptionalPermissions(const Extension& extension,
                                 const PermissionSet& permissions,
                                 RemoveType remove_type,
                                 base::OnceClosure completion_callback);

  // Removes |permissions| that were withheld at installation and granted at
  // runtime from |extension|, updating the active permission set and notifying
  // any observers.
  // NOTE: This should only be used for removing permissions through the runtime
  // host permissions feature.
  void RevokeRuntimePermissions(const Extension& extension,
                                const PermissionSet& permissions,
                                base::OnceClosure completion_callback);

  // Removes the |permissions| from |extension| and makes no effort to determine
  // if doing so is safe in the slightlest. This method shouldn't be used,
  // except for removing permissions totally blocklisted by management.
  void RemovePermissionsUnsafe(const Extension* extension,
                               const PermissionSet& permissions);

  // Fetches the policy settings from the ExtensionManagement service and
  // applies them to the extension.
  void ApplyPolicyHostRestrictions(const Extension& extension);

  // Sets list of hosts |extension| may not interact with (overrides default).
  void SetPolicyHostRestrictions(const Extension* extension,
                                 const URLPatternSet& runtime_blocked_hosts,
                                 const URLPatternSet& runtime_allowed_hosts);

  // Sets extension to use the default list of policy host restrictions.
  void SetUsesDefaultHostRestrictions(const Extension* extension);

  // Sets list of hosts extensions may not interact with. Extension specific
  // exceptions to this default policy are defined with
  // SetPolicyHostRestrictions.
  void SetDefaultPolicyHostRestrictions(
      const URLPatternSet& default_runtime_blocked_hosts,
      const URLPatternSet& default_runtime_allowed_hosts);

  // Returns the set of revokable permissions.
  std::unique_ptr<const PermissionSet> GetRevokablePermissions(
      const Extension* extension) const;

  // Adds all permissions in the |extension|'s active permissions to its
  // granted permission set.
  void GrantActivePermissions(const Extension* extension);

  // Initializes the |extension|'s active permission set to include only
  // permissions currently requested by the extension and all the permissions
  // required by the extension.
  void InitializePermissions(const Extension* extension);

  // Adds |permissions| to |extension| without doing any validation or
  // persisting values in prefs.
  // TODO(devlin): We shouldn't need this, even for tests. Tests shouldn't be
  // testing behavior that is impossible in production.
  void AddPermissionsForTesting(const Extension& extension,
                                const PermissionSet& permissions);

  static void EnsureAssociatedFactoryBuilt();

 private:
  class NetworkPermissionsUpdateHelper;

  enum EventType {
    ADDED,
    REMOVED,
    POLICY,
  };

  // A bit mask of the permission set to be updated in ExtensionPrefs.
  enum PermissionsStore {
    kNone = 0,
    kGrantedPermissions = 1 << 0,
    kRuntimeGrantedPermissions = 1 << 1,
    kActivePermissions = 1 << 2,
  };

  // Issues the relevant events, messages and notifications when the
  // |extension|'s permissions have |changed| (|changed| is the delta).
  // Specifically, this sends the EXTENSION_PERMISSIONS_UPDATED notification,
  // the UpdatePermissions Mojo message, and fires the onAdded/onRemoved events
  // in the extension.
  static void NotifyPermissionsUpdated(
      content::BrowserContext* browser_context,
      EventType event_type,
      scoped_refptr<const Extension> extension,
      std::unique_ptr<const PermissionSet> changed,
      base::OnceClosure completion_callback);

  // Issues the relevant events, messages and notifications when the default
  // scope management policy have changed.
  // Specifically, this sends the UpdateDefaultHostRestrictions Mojo message.
  static void NotifyDefaultPolicyHostRestrictionsUpdated(
      content::BrowserContext* browser_context,
      const URLPatternSet default_runtime_blocked_hosts,
      const URLPatternSet default_runtime_allowed_hosts);

  // Sets the |extension|'s active permissions to |active|, and calculates and
  // sets the |extension|'s new withheld permissions. This also calculates the
  // set of permissions to be withheld on the extension.
  void SetPermissions(const Extension* extension,
                      std::unique_ptr<const PermissionSet> active,
                      bool withhold_optional_permissions);

  // Adds the given |active_permissions_to_add| to |extension|'s current
  // active permissions (i.e., the permissions associated with the |extension|
  // object and the extension's process). Updates the preferences according to
  // |permission_store_mask| with |permissions_to_add_to_prefs|.
  // The sets of |permissions_to_add_to_prefs| and |active_permissions_to_add|
  // may differ in the case of granting a wider set of permissions than what
  // the extension explicitly requested, as described in
  // GrantRuntimePermissions().
  void AddPermissionsImpl(const Extension& extension,
                          const PermissionSet& active_permissions_to_add,
                          int prefs_permissions_store_mask,
                          const PermissionSet& permissions_to_add_to_prefs,
                          base::OnceClosure completion_callback);

  // Sets the given `extension`'s active permissions to the specified
  // `new_active_permissions`. Also removes `permissions_to_remove_from_prefs`
  // from the preferences indicated by `prefs_permissions_store_mask`. It also
  // allows withholding optional permissions if `withhold_optional_permissions`
  // is set to true. Invokes `completion_callback` when done.
  void RemovePermissionsImpl(
      const Extension& extension,
      std::unique_ptr<const PermissionSet> new_active_permissions,
      const PermissionSet& permissions_to_remove_from_prefs,
      int prefs_permissions_store_mask,
      bool withhold_optional_permissions,
      base::OnceClosure completion_callback);

  // The associated BrowserContext.
  raw_ptr<content::BrowserContext> browser_context_;

  // Initialization flag that determines whether prefs is consulted about the
  // extension. Transient extensions should not have entries in prefs.
  InitFlag init_flag_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PERMISSIONS_PERMISSIONS_UPDATER_H_
