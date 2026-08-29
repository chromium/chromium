// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_LOW_TRUST_POLICY_INSTALL_BLOCK_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_LOW_TRUST_POLICY_INSTALL_BLOCK_MANAGER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/extension_util.h"
#include "extensions/common/extension_id.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace extensions {

struct BlockedExtensionInfo {
  util::DseNtpOverrideType override_type = util::DseNtpOverrideType::kNone;
  std::string update_url;
  base::Time timestamp = base::Time::Now();
};

// Manages tracking, query, and TTL eviction of enterprise policy extension
// installations that have been blocked on unmanaged / low-trust devices.
//
// Owned by `ExtensionManagement` as a per-profile component whose lifecycle is
// bound to the associated `Profile`.
class LowTrustPolicyInstallBlockManager {
 public:
  // Registers the profile dictionary preference for tracking policy extensions
  // blocked in low-trust environments.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit LowTrustPolicyInstallBlockManager(PrefService& pref_service);
  ~LowTrustPolicyInstallBlockManager();

  LowTrustPolicyInstallBlockManager(const LowTrustPolicyInstallBlockManager&) =
      delete;
  LowTrustPolicyInstallBlockManager& operator=(
      const LowTrustPolicyInstallBlockManager&) = delete;

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

 private:
  // Holds a `raw_ref` to `PrefService` and must not outlive it.
  const raw_ref<PrefService> pref_service_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_LOW_TRUST_POLICY_INSTALL_BLOCK_MANAGER_H_
