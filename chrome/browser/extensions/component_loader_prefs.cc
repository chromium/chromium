// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/component_loader_prefs.h"

#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/common/extension_id.h"

namespace extensions::component_loader_prefs {

namespace {

// Local State preference storing a dictionary of staged component extensions,
// mapping ExtensionId to a dictionary containing the relative install path and
// parsed manifest dictionary.
constexpr char kComponentExtensionStagedUpdatesPref[] =
    "extensions.staged_component_updates";

constexpr char kRelativePathPrefKey[] = "relative_path";
constexpr char kManifestPrefKey[] = "manifest";

}  // namespace

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kComponentExtensionStagedUpdatesPref);
}

void StageExtension(PrefService& local_state,
                    const ExtensionId& extension_id,
                    const base::FilePath& relative_path,
                    base::DictValue manifest) {
  ScopedDictPrefUpdate update(&local_state,
                              kComponentExtensionStagedUpdatesPref);
  base::DictValue entry;
  entry.Set(kRelativePathPrefKey, relative_path.AsUTF8Unsafe());
  entry.Set(kManifestPrefKey, std::move(manifest));
  update->Set(extension_id, std::move(entry));
}

void ClearExtension(PrefService& local_state, const ExtensionId& extension_id) {
  if (!local_state.GetDict(kComponentExtensionStagedUpdatesPref)
           .contains(extension_id)) {
    return;
  }
  ScopedDictPrefUpdate update(&local_state,
                              kComponentExtensionStagedUpdatesPref);
  update->Remove(extension_id);
}

std::optional<StagedComponentExtensionInfo> GetExtension(
    const PrefService& local_state,
    const ExtensionId& extension_id) {
  const base::DictValue* entry =
      local_state.GetDict(kComponentExtensionStagedUpdatesPref)
          .FindDict(extension_id);
  if (!entry) {
    return std::nullopt;
  }

  const std::string* path_str = entry->FindString(kRelativePathPrefKey);
  const base::DictValue* manifest = entry->FindDict(kManifestPrefKey);
  if (!path_str || !manifest) {
    return std::nullopt;
  }

  base::FilePath relative_path = base::FilePath::FromUTF8Unsafe(*path_str);
  if (relative_path.empty() || relative_path.IsAbsolute() ||
      relative_path.ReferencesParent()) {
    return std::nullopt;
  }

  return StagedComponentExtensionInfo{std::move(relative_path),
                                      manifest->Clone()};
}

}  // namespace extensions::component_loader_prefs
