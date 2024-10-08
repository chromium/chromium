// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_MANAGER_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_MANAGER_H_

#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/management_policy.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class Extension;
class ExtensionPrefs;
class ExtensionSystem;
class ExtensionRegistry;

// UMA metrics for auto-approved extensions.
constexpr char kInitialLocallyApprovedExtensionCountWinLinuxMacHistogramName[] =
    "SupervisedUsers.InitialLocallyApprovedExtensionsCountOnWinLinuxMac";
constexpr char kExtensionApprovalsCountOnExtensionToggleHistogramName[] =
    "SupervisedUsers.ExtensionApprovalsCountOnExtensionToggle";

// This class groups all the functionality to handle extensions
// for supervised users.
class SupervisedUserExtensionsManager : public ExtensionRegistryObserver,
                                        public ManagementPolicy::Provider {
 public:
  explicit SupervisedUserExtensionsManager(content::BrowserContext* context);
  SupervisedUserExtensionsManager(const SupervisedUserExtensionsManager&) =
      delete;
  SupervisedUserExtensionsManager& operator=(
      const SupervisedUserExtensionsManager&) = delete;
  ~SupervisedUserExtensionsManager() override;

  // Updates registration of this class as a management policy provider
  // for supervised users. It needs to be called after
  // extension::ManagementPolicy has been set.
  void UpdateManagementPolicyRegistration();

  // Updates the set of approved extensions to add approval for `extension`.
  void AddExtensionApproval(const Extension& extension);

  // Checks if the extension escalated permissions during an upgrade
  // and records the corresponding metrics.
  void MaybeRecordPermissionsIncreaseMetrics(const extensions::Extension& extension);

  // Updates the set of approved extensions to remove approval for `extension`.
  void RemoveExtensionApproval(const Extension& extension);

  // Returns whether the extension is allowed by the parent.
  bool IsExtensionAllowed(const Extension& extension) const;

  // Returns whether the supervised user can install extensions.
  // If the feature that allows skipping parent approval is enabled, supervised
  // user are always allowed to install extensions.
  // If the feature is disabled, the permission to install is based on existing
  // Family Link parental controls ("Permissions" switch).
  bool CanInstallExtensions() const;

  // Records the state of extension approvals.
  void RecordExtensionEnablementUmaMetrics(bool enabled) const;

  // extensions::ManagementPolicy::Provider implementation:
  std::string GetDebugPolicyProviderName() const override;
  bool UserMayLoad(const Extension* extension,
                   std::u16string* error) const override;
  bool MustRemainDisabled(const Extension* extension,
                          disable_reason::DisableReason* reason,
                          std::u16string* error) const override;

  // extensions::ExtensionRegistryObserver overrides:
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const Extension* extension,
                            bool is_update) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              UninstallReason reason) override;

 private:
  // These enum values represent operations to manage the
  // kSupervisedUserApprovedExtensions user pref, which stores parent approved
  // extension ids.
  enum class ApprovedExtensionChange {
    // Adds a new approved extension to the pref.
    kAdd,
    // Removes extension approval.
    kRemove
  };

  // An extension can be in one of the following states:
  //
  // BLOCKED: if kSupervisedUserExtensionsMayRequestPermissions is false and the
  // child user is attempting to install a new extension or an existing
  // extension is asking for additional permissions.
  // ALLOWED: Components, Themes, Default extensions ..etc
  //    are generally allowed.  Extensions that have been approved by the
  //    custodian are also allowed.
  // REQUIRE_APPROVAL: if it is installed by the child user and
  //    hasn't been approved by the custodian yet.
  enum class ExtensionState { BLOCKED, ALLOWED, REQUIRE_APPROVAL };

  // Returns the state of an extension whether being BLOCKED, ALLOWED or
  // REQUIRE_APPROVAL from the Supervised User service's point of view.
  ExtensionState GetExtensionState(const Extension& extension) const;

  // Updates the set of approved extensions when the preference is changed.
  void RefreshApprovedExtensionsFromPrefs();

  // Activates the extension manager for supervised users.
  void SetActiveForSupervisedUsers();

  // Marks the class as active manegment policy provider for supervised users
  // and updates management policy registration.
  void ActivateManagementPolicyAndUpdateRegistration();

  // Updates the synced set of approved extension ids.
  // Use AddExtensionApproval() or RemoveExtensionApproval() for public access.
  // If `type` is kAdd, then add approval.
  // If `type` is kRemove, then remove approval.
  // Triggers a call to RefreshApprovedExtensionsFromPrefs() via a listener.
  // TODO(crbug.com/40685974): We don't need the extension version information.
  // It's only included for backwards compatibility with previous versions of
  // Chrome. Remove the version information once a sufficient number of users
  // have migrated away from M83.
  void UpdateApprovedExtension(const std::string& extension_id,
                               const std::string& version,
                               ApprovedExtensionChange type);

  // Returns a message saying that extensions can only be modified by the
  // custodian.
  std::u16string GetExtensionsLockedMessage() const;

  // Enables/Disables extensions upon change in approvals. This function is
  // idempotent.
  void ChangeExtensionStateIfNecessary(const std::string& extension_id);

  // Returns whether we should block an extension.
  // If the toggle "Permissions for sites, apps and extensions" toggle
  // is used to manage supervised user extensions, the result depends on the
  // value of the toggle.
  // If the new toggle "Extensions" is used to manage supervised user
  // extensions, this method return always false.
  // TODO(b/321239324): De-release when the "Extensions" toggle-management is
  // launched.
  bool ShouldBlockExtension(const std::string& extension_id) const;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // Triggers an one-time migration of the present extensions as parent-approved
  // when the feature
  // `kEnableSupervisedUserSkipParentApprovalToInstallExtensions` becomes
  // enabled.
  void MaybeMarkExtensionsLocallyParentApproved();

  // Marks the extensions available to the child user as locally parent-approved
  // on a preference on this device.
  void DoExtensionsMigrationToParentApproved();
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  // Returns true if the given `extension_id` has been marked locally parent
  // approved.
  bool IsLocallyParentApprovedExtension(const std::string& extension_id) const;

  // Removes from the locally approved extension preference the given
  // `extension_ids`. The input `extension_ids` doesn't have to be a subset of
  // the locally approved extensions: the method will remove those that are
  // locally approved and ignore the rest.
  void RemoveLocalParentalApproval(const std::set<std::string>& extension_ids);

  // Handles the parent-approval state of the present extensions,
  // whenever the parent changes the value of the FL "Extension" switch.
  void OnSkipParentApprovalToInstallExtensionsChanged();

  // The current state of registration of this class as a management policy.
  bool is_active_policy_for_supervised_users_ = false;

  const raw_ptr<content::BrowserContext> context_;
  raw_ptr<ExtensionPrefs> extension_prefs_;
  raw_ptr<ExtensionSystem> extension_system_;
  raw_ptr<ExtensionRegistry> extension_registry_;
  raw_ptr<PrefService> user_prefs_;

  PrefChangeRegistrar pref_change_registrar_;

  // Store a set of extension ids approved by the custodian.
  // It is only relevant for SU-initiated installs.
  std::set<std::string> approved_extensions_set_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_MANAGER_H_
