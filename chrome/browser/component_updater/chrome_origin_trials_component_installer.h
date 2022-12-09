// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_CHROME_ORIGIN_TRIALS_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_CHROME_ORIGIN_TRIALS_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/installer_policies/origin_trials_component_installer.h"

namespace component_updater {

class ComponentUpdateService;

class ChromeOriginTrialsComponentInstallerPolicy
    : public OriginTrialsComponentInstallerPolicy {
 public:
  ChromeOriginTrialsComponentInstallerPolicy() = default;
  ~ChromeOriginTrialsComponentInstallerPolicy() override = default;
  ChromeOriginTrialsComponentInstallerPolicy(
      const ChromeOriginTrialsComponentInstallerPolicy&) = delete;
  ChromeOriginTrialsComponentInstallerPolicy& operator=(
      const ChromeOriginTrialsComponentInstallerPolicy&) = delete;

 private:
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value::Dict manifest) override;
};

// Call once during startup to make the component update service aware of
// the origin trials update component.
void RegisterOriginTrialsComponent(ComponentUpdateService* updater_service);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_CHROME_ORIGIN_TRIALS_COMPONENT_INSTALLER_H_
