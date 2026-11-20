// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/component_updater_library_source.h"

#include <algorithm>
#include <optional>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/translate_kit_component_installer.h"
#include "chrome/browser/on_device_translation/component_update_util.h"
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
  if (update_in_progress_) {
    return;
  }
  component_updater::ComponentUpdateService* cus =
      g_browser_process->component_updater();
  if (std::ranges::contains(
          cus->GetComponentIDs(),
          component_updater::TranslateKitComponentInstallerPolicy::
              GetExtensionId())) {
    // The component was registered before, for example at startup, but it
    // isn't installed. Ask for it now instead of waiting for the next
    // periodic update check.
    UpdateOnDemand();
    return;
  }
  PrefService* local_state = pref_change_registrar_.prefs();
  CHECK(local_state);
  // `update_in_progress_` makes sure that only one on-demand update runs
  // until registration finishes.
  update_in_progress_ = true;
  component_updater::RegisterTranslateKitComponent(
      cus, local_state,
      /*force_install=*/true,
      /*registered_callback=*/
      base::BindOnce(
          [](base::WeakPtr<ComponentUpdaterLibrarySource> source) {
            if (source) {
              source->update_in_progress_ = false;
              source->UpdateOnDemand();
            }
          },
          weak_ptr_factory_.GetWeakPtr()),
      // The client learns about the installed library from the pref change.
      /*on_ready_callback=*/base::DoNothing());
}

void ComponentUpdaterLibrarySource::UpdateOnDemand() {
  if (update_in_progress_) {
    return;
  }
  update_in_progress_ = true;
  component_updater::TranslateKitComponentInstallerPolicy::
      UpdateComponentOnDemand(
          g_browser_process->component_updater(),
          base::BindOnce(&ComponentUpdaterLibrarySource::OnUpdateFinished,
                         weak_ptr_factory_.GetWeakPtr()));
}

void ComponentUpdaterLibrarySource::OnUpdateFinished(
    update_client::Error error) {
  update_in_progress_ = false;
  std::optional<InstallError> install_error = GetOnDemandUpdateError(
      g_browser_process->component_updater(),
      component_updater::TranslateKitComponentInstallerPolicy::GetExtensionId(),
      error, !GetLibraryPath().empty());
  if (install_error && client()) {
    client()->OnLibraryInstallFailed(*install_error);
  }
}

void ComponentUpdaterLibrarySource::OnLibraryPathPrefChanged() {
  if (client()) {
    client()->OnLibraryChanged();
  }
}

}  // namespace on_device_translation
