// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/aw_component_installer_policy_delegate.h"

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "android_webview/nonembedded/component_updater/aw_component_update_service.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/notreached.h"
#include "base/values.h"
#include "base/version.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"

namespace android_webview {

AwComponentInstallerPolicyDelegate::AwComponentInstallerPolicyDelegate() =
    default;

AwComponentInstallerPolicyDelegate::~AwComponentInstallerPolicyDelegate() =
    default;

update_client::CrxInstaller::Result
AwComponentInstallerPolicyDelegate::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir,
    const std::vector<uint8_t>& hash) {
  std::string version_ascii;
  manifest.GetStringASCII("version", &version_ascii);
  const base::Version version(version_ascii);
  if (!version.IsValid()) {
    return update_client::CrxInstaller::Result(
        update_client::InstallError::INVALID_VERSION);
  }

  // Scoped temp dir, the directory is created under the app cache dir.
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    return update_client::CrxInstaller::Result(
        update_client::InstallError::MOVE_FILES_ERROR);
  }

  const base::FilePath temp_path =
      temp_dir.GetPath().AppendASCII(version_ascii);
  // Copying files to a temp path before passing it to the
  // ComponentProviderService. This way, the ComponentUpdateService can safely
  // delete those files even if the ComponentProviderService hasn't finished
  // copying them yet.
  // TODO(crbug.com/1176335) use file links to avoid copying files.
  if (!base::CopyDirectory(install_dir, temp_path,
                           /* recursive= */ true)) {
    return update_client::CrxInstaller::Result(
        update_client::InstallError::MOVE_FILES_ERROR);
  }

  // ComponentProviderService should take ownership of the temp path.
  if (AwComponentUpdateService::GetInstance()->NotifyNewVersion(
          update_client::GetCrxIdFromPublicKeyHash(hash), temp_path, version)) {
    return update_client::CrxInstaller::Result(
        update_client::InstallError::NONE);
  }

  return update_client::CrxInstaller::Result(
      update_client::InstallError::GENERIC_ERROR);
}

void AwComponentInstallerPolicyDelegate::OnCustomUninstall() {
  // Uninstallation isn't supported in WebView.
  NOTREACHED();
}

void AwComponentInstallerPolicyDelegate::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  // This is usually when components files are parsed and loaded into memory.
  // This has to be a NOOP because this runs in a nonembedded WebView process
  // outside of a browser context.
}

}  // namespace android_webview
