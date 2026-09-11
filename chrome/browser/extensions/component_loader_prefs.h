// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_COMPONENT_LOADER_PREFS_H_
#define CHROME_BROWSER_EXTENSIONS_COMPONENT_LOADER_PREFS_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/values.h"
#include "extensions/common/extension_id.h"

class PrefRegistrySimple;
class PrefService;

// Helpers for managing Local State preferences for component extensions that
// can be updated via the component updater.
//
// When the component updater installs a newer version of an extension during a
// browser session, it stages the relative install path and manifest in Local
// State. On the next browser startup, `ComponentLoader` checks these
// preferences and loads the staged version from disk instead of the bundled
// resource.
namespace extensions::component_loader_prefs {

// Relative path and manifest of a component extension staged in Local State to
// be loaded on the next browser startup.
struct StagedComponentExtensionInfo {
  // Install path of the staged extension relative to DIR_COMPONENT_USER.
  base::FilePath relative_path;

  // Parsed manifest dictionary of the staged extension.
  base::DictValue manifest;
};

// Registers the Local State preference dictionary used to track staged
// component extensions.
void RegisterPrefs(PrefRegistrySimple* registry);

// Records `relative_path` and `manifest` for `extension_id` in `local_state` so
// `ComponentLoader` will load it on the next browser startup.
void StageExtension(PrefService& local_state,
                    const ExtensionId& extension_id,
                    const base::FilePath& relative_path,
                    base::DictValue manifest);

// Clears any staged information for `extension_id` in `local_state`.
void ClearExtension(PrefService& local_state, const ExtensionId& extension_id);

// Returns the staged relative path and manifest for `extension_id` in
// `local_state` if valid, or `std::nullopt` otherwise.
std::optional<StagedComponentExtensionInfo> GetExtension(
    const PrefService& local_state,
    const ExtensionId& extension_id);

}  // namespace extensions::component_loader_prefs

#endif  // CHROME_BROWSER_EXTENSIONS_COMPONENT_LOADER_PREFS_H_
