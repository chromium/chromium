// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/component_updater_library_source.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/translate_kit_component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/on_device_translation/public/pref_names.h"
#include "components/prefs/pref_service.h"

namespace on_device_translation {

ComponentUpdaterLibrarySource::ComponentUpdaterLibrarySource(
    PrefService* local_state) {
  CHECK(local_state);
  pref_change_registrar_.Init(local_state);
  pref_change_registrar_.Add(
      prefs::kTranslateKitBinaryPath,
      base::BindRepeating(
          &ComponentUpdaterLibrarySource::OnLibraryPathPrefChanged,
          base::Unretained(this)));
}

ComponentUpdaterLibrarySource::~ComponentUpdaterLibrarySource() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

base::FilePath ComponentUpdaterLibrarySource::GetLibraryPath() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const PrefService* local_state = pref_change_registrar_.prefs();
  if (!local_state) {
    return base::FilePath();
  }
  base::FilePath library_path =
      local_state->GetFilePath(prefs::kTranslateKitBinaryPath);
  // Earlier versions stored a relative path in the pref when they registered
  // the component, before it was installed. Treat any relative path as "not
  // installed", so that profiles with that value wait for the install.
  if (!library_path.IsAbsolute()) {
    return base::FilePath();
  }
  return library_path;
}

void ComponentUpdaterLibrarySource::Install() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PrefService* local_state = pref_change_registrar_.prefs();
  CHECK(local_state);
  component_updater::ComponentUpdateService* cus =
      g_browser_process->component_updater();
  component_updater::RegisterTranslateKitComponent(
      cus, local_state,
      /*force_install=*/true,
      /*registered_callback=*/
      base::BindOnce(&component_updater::TranslateKitComponentInstallerPolicy::
                         UpdateComponentOnDemand,
                     base::Unretained(cus)),
      // The client learns about the installed library from the pref change.
      /*on_ready_callback=*/base::DoNothing());
}

void ComponentUpdaterLibrarySource::OnLibraryPathPrefChanged() {
  if (client()) {
    client()->OnLibraryChanged();
  }
}

}  // namespace on_device_translation
