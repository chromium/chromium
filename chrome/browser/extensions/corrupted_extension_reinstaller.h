// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CORRUPTED_EXTENSION_REINSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_CORRUPTED_EXTENSION_REINSTALLER_H_

#include <map>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "net/base/backoff_entry.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Class that asks ExtensionService to reinstall corrupted extensions.
// If a reinstallation fails for some reason (e.g. network unavailability) then
// it will retry reinstallation with backoff.
class CorruptedExtensionReinstaller {
 public:
  // The reason why we want to reinstall the extension.
  // Note: enum used for UMA. Do NOT reorder or remove entries. Don't forget to
  // update enums.xml (name: ExtensionPolicyReinstallReason) when adding new
  // entries.
  enum class PolicyReinstallReason {
    // Tried to load extension which was previously disabled because of
    // corruption (but is a force-installed extension and therefore should be
    // repaired).
    // That happens when extension corruption was detected, but for some reason
    // reinstall could not happen in the same session (no internet or session
    // was closed right after detection), so at start of the next session we add
    // extension to reinstall list again.
    CORRUPTION_DETECTED_IN_PRIOR_SESSION = 0,

    // Corruption detected in an extension from Chrome Web Store.
    CORRUPTION_DETECTED_WEBSTORE = 1,

    // Corruption detected in an extension outside Chrome Web Store.
    CORRUPTION_DETECTED_NON_WEBSTORE = 2,

    // Planned future option:
    // Extension doesn't have hashes for corruption checks. This should not
    // happen for extension from Chrome Web Store (since we can fetch hashes
    // from server), but for extensions outside Chrome Web Store that means that
    // we need to reinstall the extension (and compute hashes during
    // installation).
    // Not used currently, see https://crbug.com/958794#c22 for details.
    // NO_UNSIGNED_HASHES_FOR_NON_WEBSTORE = 3,

    // Extension doesn't have hashes for corruption checks. Ideally this
    // extension should be reinstalled in this case, but currently we just skip
    // them. See https://crbug.com/958794#c22 for details.
    NO_UNSIGNED_HASHES_FOR_NON_WEBSTORE_SKIP = 4,

    // Magic constant used by the histogram macros.
    // Always update it to the max value.
    kMaxValue = NO_UNSIGNED_HASHES_FOR_NON_WEBSTORE_SKIP
  };

  using ReinstallCallback =
      base::RepeatingCallback<void(base::OnceClosure callback,
                                   base::TimeDelta delay)>;

  explicit CorruptedExtensionReinstaller(content::BrowserContext* context);

  CorruptedExtensionReinstaller(const CorruptedExtensionReinstaller&) = delete;
  CorruptedExtensionReinstaller& operator=(
      const CorruptedExtensionReinstaller&) = delete;

  ~CorruptedExtensionReinstaller();

  // Records UMA metrics about policy reinstall to UMA. Temporarily exposed
  // publicly because we now skip reinstall for non-webstore policy
  // force-installed extensions without hashes, but are interested in number
  // of such cases.
  // See https://crbug.com/958794#c22 for details.
  void RecordPolicyReinstallReason(PolicyReinstallReason reason_for_uma);

  // Notifies the manager that we are reinstalling the policy force-installed
  // extension with |id| because we detected corruption in the current copy.
  // |reason_for_uma| indicates origin and details of the requires, and is used
  // for statistics purposes (sent to UMA). |manifest_location_for_uma| is the
  // manifest location, and is used for statistics purposes (sent to UMA)
  void ExpectReinstallForCorruption(
      const ExtensionId& id,
      std::optional<PolicyReinstallReason> reason_for_uma,
      mojom::ManifestLocation manifest_location_for_uma);

  // Call this method when extension in reinstalled to remove it from the set
  // and update the metrics.
  void MarkResolved(const ExtensionId& id);

  // Returns true if we are expecting a reinstall of the extension with |id| due
  // to corruption?
  bool IsReinstallForCorruptionExpected(const ExtensionId& id) const;

  // Whether or not there are any corrupted extensions.
  bool HasAnyReinstallForCorruption() const;

  // Gets the view on extensions scheduled for reinstall.
  const std::map<ExtensionId, base::TimeTicks>& GetExpectedReinstalls() const;

  // Notifies this reinstaller about an extension corruption.
  void NotifyExtensionDisabledDueToCorruption();

  // Called when ExtensionSystem is shutting down. Cancels already-scheduled
  // attempts, if any, for a smoother shutdown.
  void Shutdown();

  // For tests, overrides the default action to take to initiate reinstalls.
  static void set_reinstall_action_for_test(ReinstallCallback* action);

 private:
  void Fire();
  base::TimeDelta GetNextFireDelay();
  void ScheduleNextReinstallAttempt();

  const raw_ptr<content::BrowserContext, DanglingUntriaged> context_ = nullptr;

  // A set of extension ids that are being reinstalled due to corruption, mapped
  // to the time we detected the corruption.
  std::map<ExtensionId, base::TimeTicks> expected_reinstalls_;

  net::BackoffEntry backoff_entry_;
  // Whether or not there is a pending PostTask to Fire().
  bool scheduled_fire_pending_ = false;

  base::WeakPtrFactory<CorruptedExtensionReinstaller> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CORRUPTED_EXTENSION_REINSTALLER_H_
