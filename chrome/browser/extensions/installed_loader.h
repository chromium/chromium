// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_INSTALLED_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_INSTALLED_LOADER_H_

#include <set>

#include "base/files/file_path.h"

namespace extensions {

class ExtensionPrefs;
class ExtensionRegistry;
class ExtensionService;
struct ExtensionInfo;

// Loads installed extensions from the prefs.
class InstalledLoader {
 public:
  explicit InstalledLoader(ExtensionService* extension_service);
  virtual ~InstalledLoader();

  // Loads extension from prefs.
  void Load(const ExtensionInfo& info, bool write_to_prefs);

  // Loads all installed extensions (used by startup and testing code).
  void LoadAllExtensions();

  // Allows tests to verify metrics without needing to go through
  // LoadAllExtensions().
  void RecordExtensionsMetricsForTesting();

 private:
  // Returns the flags that should be used with Extension::Create() for an
  // extension that is already installed.
  int GetCreationFlags(const ExtensionInfo* info);

  // Record metrics related to the loaded extensions.
  void RecordExtensionsMetrics();

  ExtensionService* extension_service_;
  ExtensionRegistry* extension_registry_;

  ExtensionPrefs* extension_prefs_;

  // Paths to invalid extension manifests, which should not be loaded.
  std::set<base::FilePath> invalid_extensions_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_INSTALLED_LOADER_H_
