// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_LOW_TRUST_POLICY_INSTALL_BLOCK_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_LOW_TRUST_POLICY_INSTALL_BLOCK_MANAGER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_util.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "extensions/common/extension_id.h"

class PrefService;

namespace content {
class BrowserContext;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace extensions {

struct BlockedExtensionInfo {
  util::DseNtpOverrideType override_type = util::DseNtpOverrideType::kNone;
  std::string update_url;
  base::Time timestamp = base::Time::Now();
};

// Manages tracking, query, TTL eviction, and uninstallation of enterprise
// policy extension installations that have been blocked on unmanaged /
// low-trust devices.
//
// Owned by `ExtensionManagement` as a per-profile component whose lifecycle is
// bound to the associated `Profile`.
class LowTrustPolicyInstallBlockManager
    : public ExtensionManagement::Observer,
      public policy::PolicyService::Observer {
 public:
  // Registers the profile dictionary preference for tracking policy extensions
  // blocked in low-trust environments.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  LowTrustPolicyInstallBlockManager(content::BrowserContext* context,
                                    PrefService& pref_service,
                                    ExtensionManagement& extension_management);
  ~LowTrustPolicyInstallBlockManager() override;

  LowTrustPolicyInstallBlockManager(const LowTrustPolicyInstallBlockManager&) =
      delete;
  LowTrustPolicyInstallBlockManager& operator=(
      const LowTrustPolicyInstallBlockManager&) = delete;

  // ExtensionManagement::Observer:
  void OnExtensionManagementSettingsChanged() override;

  // policy::PolicyService::Observer:
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;
  void OnPolicyServiceInitialized(policy::PolicyDomain domain) override;

  // Records an enterprise policy extension installation as blocked by low
  // trust in preferences, saving its metadata and timestamp for TTL expiration
  // checks.
  void MarkBlocked(const ExtensionId& extension_id,
                   const BlockedExtensionInfo& info);

  // Removes an enterprise policy extension's low-trust blocked status from
  // preferences.
  void Clear(const ExtensionId& extension_id);

  // Checks if an enterprise policy installation is actively marked as blocked
  // in preferences. Returns true only if the record exists and is within the
  // TTL window.
  bool IsBlocked(const ExtensionId& extension_id) const;

  // Retrieves all active low-trust blocked policy extension records within
  // the TTL window from preferences.
  base::flat_map<ExtensionId, BlockedExtensionInfo> GetAllBlocked() const;

  // Scans preferences and removes all low-trust blocked policy extension
  // records that have exceeded the TTL, have invalid enum values, or are
  // malformed. Returns the number of evicted entries.
  size_t CleanupStaleRecords();

  // Returns the TTL duration for low-trust blocked policy extension entries.
  // Provided for unit testing TTL eviction behavior.
  static base::TimeDelta GetTTLForTesting();

  void SetPolicyServiceForTesting(policy::PolicyService* policy_service);

 private:
  // Subscribes to ExtensionSystem::ready() once ExtensionManagement
  // construction completes. When ExtensionSystem signals that all installed
  // extensions are loaded into memory, this triggers
  // OnExtensionManagementSettingsChanged() to purge blocked cache entries for
  // policies that no longer exist and uninstall extensions that violate policy.
  void CleanupBlockedCacheAndExtensionsWhenReady();

  // Scans the blocked cache and removes entries for extensions that are no
  // longer configured as forced or recommended installs in enterprise policy.
  // Runs unconditionally (even when blocking is inactive) to maintain cache
  // hygiene when policies are removed.
  void CleanupRemovedPolicyRecords();

  // Uninstalls policy-installed DSE and NTP override extensions when running
  // in a low-trust environment to protect consumer devices from persistent
  // overrides after trust is lost, and records them in the blocked cache.
  void UninstallBlockedExtensions();

  policy::PolicyService* policy_service() const;

  // Associated browser context. Guaranteed to be non-null and to outlive this
  // object.
  const raw_ptr<content::BrowserContext> context_;

  // Holds a `raw_ref` to `PrefService` and must not outlive it.
  const raw_ref<PrefService> pref_service_;

  // Reference to the owning ExtensionManagement instance. Guaranteed to outlive
  // this object because this manager is owned exclusively by it.
  const raw_ref<ExtensionManagement> extension_management_;

  raw_ptr<policy::PolicyService> policy_service_for_testing_ = nullptr;
  bool observing_policy_service_ = false;

  base::ScopedObservation<ExtensionManagement, ExtensionManagement::Observer>
      extension_management_observation_{this};

  base::WeakPtrFactory<LowTrustPolicyInstallBlockManager> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_LOW_TRUST_POLICY_INSTALL_BLOCK_MANAGER_H_
