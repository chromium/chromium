// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_PRIVACY_SANDBOX_ATTESTATIONS_COMPONENT_INSTALLER_TEST_UTIL_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_PRIVACY_SANDBOX_ATTESTATIONS_COMPONENT_INSTALLER_TEST_UTIL_H_

#include "chrome/browser/component_updater/privacy_sandbox_attestations_component_installer.h"

#include <string_view>

namespace base {
class FilePath;
class Version;
}

namespace privacy_sandbox {
class PrivacySandboxAttestationsProto;
}  // namespace privacy_sandbox

namespace component_updater {

// Create an attestations file under directory `install_dir`. Write `contents`
// to it. Returns true only if the write is successful. Only used in tests.
bool WritePrivacySandboxAttestationsFileForTesting(
    const base::FilePath& install_dir,
    std::string_view contents);

// Install attestations component under DIR_COMPONENT_USER directory with
// contents `proto`.
bool InstallPrivacySandboxAttestationsComponentForTesting(
    const privacy_sandbox::PrivacySandboxAttestationsProto& proto,
    const base::Version& version);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_PRIVACY_SANDBOX_ATTESTATIONS_COMPONENT_INSTALLER_TEST_UTIL_H_
