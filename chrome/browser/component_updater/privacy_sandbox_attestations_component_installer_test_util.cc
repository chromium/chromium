// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/privacy_sandbox_attestations_component_installer_test_util.h"

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/proto/privacy_sandbox_attestations.pb.h"

namespace component_updater {

using Installer = PrivacySandboxAttestationsComponentInstallerPolicy;

bool WritePrivacySandboxAttestationsFileForTesting(
    const base::FilePath& install_dir,
    std::string_view contents) {
  return base::WriteFile(Installer::GetInstalledFilePath(install_dir),
                         contents);
}

bool InstallPrivacySandboxAttestationsComponentForTesting(
    const privacy_sandbox::PrivacySandboxAttestationsProto& proto,
    const base::Version& version) {
  // Serialize to string.
  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);

  return InstallPrivacySandboxAttestationsComponentForTesting(serialized_proto,
                                                              version);
}

bool InstallPrivacySandboxAttestationsComponentForTesting(
    std::string_view contents,
    const base::Version& version) {
  // Allow blocking for file IO.
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Write the serialized proto to the attestation list file.
  base::FilePath install_dir =
      GetPrivacySandboxAtteststionsComponentInstallDir(version);
  if (!base::CreateDirectory(install_dir)) {
    return false;
  }

  if (!WritePrivacySandboxAttestationsFileForTesting(install_dir, contents)) {
    return false;
  }

  // Write a manifest file. This is needed for component updater to detect any
  // existing component on disk.
  return base::WriteFile(
      install_dir.Append(FILE_PATH_LITERAL("manifest.json")),
      base::ReplaceStringPlaceholders(
          R"({
               "manifest_version": 1,
               "name": "Privacy Sandbox Attestations",
               "version": "$1"
              })",
          /*subst=*/{version.GetString()}, /*offsets=*/nullptr));
}

base::FilePath GetPrivacySandboxAtteststionsComponentInstallDir(
    const base::Version& version) {
  // Get component updater directory that contains user-wide components.
  base::FilePath component_updater_dir;
  base::PathService::Get(DIR_COMPONENT_USER, &component_updater_dir);

  CHECK(!component_updater_dir.empty());

  // Write the serialized proto to the attestation list file.
  return Installer::GetInstalledDirectory(component_updater_dir)
      .AppendASCII(version.GetString());
}

}  // namespace component_updater
