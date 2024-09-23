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
    const base::Version& version,
    bool is_pre_installed) {
  // Serialize to string.
  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);

  return InstallPrivacySandboxAttestationsComponentForTesting(
      serialized_proto, version, is_pre_installed);
}

bool InstallPrivacySandboxAttestationsComponentForTesting(
    std::string_view contents,
    const base::Version& version,
    bool is_pre_installed) {
  // Allow blocking for file IO.
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Write the serialized proto to the attestation list file.
  base::FilePath install_dir = GetPrivacySandboxAtteststionsComponentInstallDir(
      version, is_pre_installed);
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
               "version": "$1",
               "pre_installed": $2
              })",
          /*subst=*/
          {version.GetString(), is_pre_installed ? "true" : "false"},
          /*offsets=*/nullptr));
}

base::FilePath GetPrivacySandboxAtteststionsComponentInstallDir(
    const base::Version& version,
    bool is_pre_installed) {
  // Get component updater directory according to whether this is a
  // pre-installed component.
  base::FilePath component_updater_dir;
  base::PathService::Get(is_pre_installed
                             ? component_updater::DIR_COMPONENT_PREINSTALLED
                             : component_updater::DIR_COMPONENT_USER,
                         &component_updater_dir);

  CHECK(!component_updater_dir.empty());

  base::FilePath install_dir =
      Installer::GetInstalledDirectory(component_updater_dir);

  if (!is_pre_installed) {
    // If this is a downloaded attestation file, it needs to be put inside a
    // directory that is named using its version number. Otherwise, for the
    // pre-install directory, the component files reside directly under it.
    install_dir = install_dir.AppendASCII(version.GetString());
  }

  // Write the serialized proto to the attestation list file.
  return install_dir;
}

}  // namespace component_updater
