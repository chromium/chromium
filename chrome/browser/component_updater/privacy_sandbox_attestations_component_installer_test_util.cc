// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/privacy_sandbox_attestations_component_installer_test_util.h"

#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"

namespace component_updater {

using Installer = PrivacySandboxAttestationsComponentInstallerPolicy;

bool WritePrivacySandboxAttestationsFileForTesting(
    const base::FilePath& install_dir,
    std::string_view contents) {
  return base::WriteFile(Installer::GetInstalledFilePath(install_dir),
                         contents);
}

}  // namespace component_updater
