// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_INSTALLED_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_INSTALLED_LOADER_H_

#include <set>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class Profile;

namespace extensions {

class Extension;
class ExtensionPrefs;
class ExtensionRegistry;
struct ExtensionInfo;
class ExtensionManagement;
class ManagementPolicy;

// Used in histogram Extensions.HostPermissions.GrantedAccess,
// Extensions.HostPermissions.GrantedAccessForBroadRequests and
// Extensions.HostPermissions.GrantedAccessForTargetedRequests.
// Entries should not be renumbered and numeric values should never be reused.
// If you are adding to this enum, update HostPermissionAccess enum in
// tools/metrics/histograms/enums.xml.
enum class HostPermissionsAccess {
  kCannotAffect = 0,
  kNotRequested = 1,
  kOnClick = 2,
  kOnSpecificSites = 3,
  kOnAllRequestedSites = 4,
  kOnActiveTabOnly = 5,
  kMaxValue = kOnActiveTabOnly,
};

// Loads installed extensions from the prefs.
class InstalledLoader {
 public:
  explicit InstalledLoader(Profile* profile);
  virtual ~InstalledLoader();

  // Loads extension from prefs.
  void Load(const ExtensionInfo& info, bool write_to_prefs);

  // Loads all installed extensions (used by startup and testing code).
  void LoadAllExtensions();

  // Loads all installed extensions (used by testing code).
  void LoadAllExtensions(Profile* profile);

  // Record a histogram using the PermissionMessage enum values for each
  // permission in `extension`. Meant to be called only for profiles where users
  // can install extensions, specifically profiles that can have non-component
  // extensions installed.
  // NOTE: If this is ever called with high frequency, the implementation may
  // need to be made more efficient.
  static void RecordPermissionMessagesHistogram(const Extension* extension,
                                                const char* histogram,
                                                Profile* profile);

  // Allows tests to verify metrics without needing to go through
  // LoadAllExtensions().
  void RecordExtensionsMetricsForTesting();

  // Allows tests to verify incremented metrics.
  void RecordExtensionsIncrementedMetricsForTesting(Profile* profile);

 private:
  // Returns the flags that should be used with Extension::Create() for an
  // extension that is already installed.
  int GetCreationFlags(const ExtensionInfo* info);

  // Records metrics related to the loaded extensions. Meant to be called only
  // for profiles where users can install extensions, specifically profiles that
  // can have non-component extensions installed.
  void RecordExtensionsMetrics(Profile* profile);

  // Handles a load request for a corrupted extension.
  void HandleCorruptExtension(const Extension& extension,
                              const ManagementPolicy& policy);

  // Returns true if this extension's update URL is from webstore, including any
  // policy overrides.
  bool UpdatesFromWebstore(const Extension& extension);

  raw_ptr<Profile> profile_;
  raw_ptr<ExtensionRegistry> extension_registry_;

  raw_ptr<ExtensionPrefs> extension_prefs_;

  // ExtensionManager pointer is cached for performance as we loop through
  // extensions.
  raw_ptr<ExtensionManagement> extension_management_ = nullptr;

  // Paths to invalid extension manifests, which should not be loaded.
  std::set<base::FilePath> invalid_extensions_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_INSTALLED_LOADER_H_
