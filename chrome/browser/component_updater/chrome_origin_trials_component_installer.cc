// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/chrome_origin_trials_component_installer.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/browser_process.h"
#include "components/embedder_support/origin_trials/component_updater_utils.h"

namespace component_updater {

void ChromeOriginTrialsComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  // Read the configuration from the manifest and set values in browser
  // local_state. These will be used on the next browser restart.
  // If an individual configuration value is missing, treat as a reset to the
  // browser defaults.
  embedder_support::ReadOriginTrialsConfigAndPopulateLocalState(
      g_browser_process->local_state(), std::move(manifest));
}

void RegisterOriginTrialsComponent(ComponentUpdateService* updater_service) {
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<ChromeOriginTrialsComponentInstallerPolicy>());
  installer->Register(updater_service, base::OnceClosure());
}

}  // namespace component_updater
