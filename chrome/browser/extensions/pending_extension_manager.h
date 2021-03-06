// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PENDING_EXTENSION_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_PENDING_EXTENSION_MANAGER_H_

#include <list>
#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/pending_extension_info.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"

class GURL;

namespace base {
class Version;
}

namespace content {
class BrowserContext;
}

namespace extensions {
FORWARD_DECLARE_TEST(ExtensionServiceTest,
                     UpdatePendingExtensionAlreadyInstalled);

class PendingExtensionManager;

class ExtensionUpdaterTest;
void SetupPendingExtensionManagerForTest(
    int count, const GURL& update_url,
    PendingExtensionManager* pending_extension_manager);

// Class PendingExtensionManager manages the set of extensions which are
// being installed or updated. In general, installation and updates take
// time, because they involve downloading, unpacking, and installing.
// This class allows us to avoid race cases where multiple sources install
// the same extension.
// The ExtensionService creates an instance of this class, and manages its
// lifetime. This class should only be used from the UI thread.
class PendingExtensionManager {
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

  explicit PendingExtensionManager(content::BrowserContext* context);
  ~PendingExtensionManager();

  // TODO(skerner): Many of these methods can be private once code in
  // ExtensionService is moved into methods of this class.

  // Remove extension with id |id| from the set of pending extensions. Returns
  // true if such an extension was found and removed, false otherwise.
  bool Remove(const std::string& id);

  // Get the  information for a pending extension.  Returns a pointer to the
  // pending extension with id |id|, or NULL if there is no such extension.
  const PendingExtensionInfo* GetById(const std::string& id) const;

  // Is |id| in the set of pending extensions?
  bool IsIdPending(const std::string& id) const;

  // Returns true if there are any extensions pending.
  bool HasPendingExtensions() const;

  // Whether there is pending extension install from sync.
  bool HasPendingExtensionFromSync() const;

  // Whether there is a high-priority pending extension (one from either policy
  // or an external component extension).
  bool HasHighPriorityPendingExtension() const;

  // Records UMA metrics about policy reinstall to UMA. Temporarily exposed
  // publicly because we now skip reinstall for non-webstore policy
  // force-installed extensions without hashes, but are interested in number
  // of such cases.
  // See https://crbug.com/958794#c22 for details.
  void RecordPolicyReinstallReason(PolicyReinstallReason reason_for_uma);

  // Notifies the manager that we are reinstalling the policy force-installed
  // extension with |id| because we detected corruption in the current copy.
  // |reason| indicates origin and details of the requires, and is used for
  // statistics purposes (sent to UMA).
  void ExpectPolicyReinstallForCorruption(const ExtensionId& id,
                                          PolicyReinstallReason reason_for_uma);

  // Are we expecting a reinstall of the extension with |id| due to corruption?
  bool IsPolicyReinstallForCorruptionExpected(const ExtensionId& id) const;

  // Whether or not there are any corrupted policy extensions.
  bool HasAnyPolicyReinstallForCorruption() const;

  // Adds an extension in a pending state; the extension with the
  // given info will be installed on the next auto-update cycle.
  // Return true if the extension was added.  Will return false
  // if the extension is pending from another source which overrides
  // sync installs (such as a policy extension) or if the extension
  // is already installed.
  // After installation, the extension will be granted permissions iff
  // |version| is valid and matches the actual installed version.
  bool AddFromSync(
      const std::string& id,
      const GURL& update_url,
      const base::Version& version,
      PendingExtensionInfo::ShouldAllowInstallPredicate should_allow_install,
      bool remote_install);

  // Adds an extension that was depended on by another extension.
  bool AddFromExtensionImport(
      const std::string& id,
      const GURL& update_url,
      PendingExtensionInfo::ShouldAllowInstallPredicate should_allow_install);

  // Given an extension id and an update URL, schedule the extension
  // to be fetched, installed, and activated.
  bool AddFromExternalUpdateUrl(const std::string& id,
                                const std::string& install_parameter,
                                const GURL& update_url,
                                Manifest::Location location,
                                int creation_flags,
                                bool mark_acknowledged);

  // Add a pending extension record for an external CRX file.
  // Return true if the CRX should be installed, false if an existing
  // pending record overrides it.
  bool AddFromExternalFile(
      const std::string& id,
      Manifest::Location location,
      const base::Version& version,
      int creation_flags,
      bool mark_acknowledged);

  // Get the list of pending IDs that should be installed from an update URL.
  // Pending extensions that will be installed from local files will not be
  // included in the set.
  void GetPendingIdsForUpdateCheck(
      std::list<std::string>* out_ids_for_update_check) const;

 private:
  typedef std::list<PendingExtensionInfo> PendingExtensionList;

  // Assumes an extension with id |id| is not already installed.
  // Return true if the extension was added.
  bool AddExtensionImpl(
      const std::string& id,
      const std::string& install_parameter,
      const GURL& update_url,
      const base::Version& version,
      PendingExtensionInfo::ShouldAllowInstallPredicate should_allow_install,
      bool is_from_sync,
      Manifest::Location install_source,
      int creation_flags,
      bool mark_acknowledged,
      bool remote_install);

  // Add a pending extension record directly.  Used for unit tests that need
  // to set an inital state. Use friendship to allow the tests to call this
  // method.
  void AddForTesting(const PendingExtensionInfo& pending_extension_info);

  // The BrowserContext with which the manager is associated.
  content::BrowserContext* context_;

  PendingExtensionList pending_extension_list_;

  // A set of policy force-installed extension ids that are being reinstalled
  // due to corruption, mapped to the time we detected the corruption.
  std::map<ExtensionId, base::TimeTicks> expected_policy_reinstalls_;

  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           UpdatePendingExtensionAlreadyInstalled);
  friend class ExtensionUpdaterTest;
  friend void SetupPendingExtensionManagerForTest(
      int count, const GURL& update_url,
      PendingExtensionManager* pending_extension_manager);

  DISALLOW_COPY_AND_ASSIGN(PendingExtensionManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PENDING_EXTENSION_MANAGER_H_
