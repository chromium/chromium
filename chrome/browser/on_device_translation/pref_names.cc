// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/pref_names.h"

#include "base/files/file_path.h"
#include "chrome/browser/on_device_translation/language_pack_util.h"
#include "components/prefs/pref_registry_simple.h"

namespace prefs {

// The fully-qualified path to the installed TranslateKit binary.
const char kTranslateKitBinaryPath[] =
    "on_device_translation.translate_kit_binary_path";

}  // namespace prefs

namespace on_device_translation {

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterFilePathPref(prefs::kTranslateKitBinaryPath,
                                 base::FilePath());

  // Register language pack config path preferences.
  for (const auto& it :
       on_device_translation::kLanguagePackComponentConfigMap) {
    registry->RegisterFilePathPref(it.second->config_path_pref,
                                   base::FilePath());
  }
}

}  // namespace on_device_translation
