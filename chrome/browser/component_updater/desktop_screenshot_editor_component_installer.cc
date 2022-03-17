// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/desktop_screenshot_editor_component_installer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "chrome/browser/share/share_features.h"
#include "components/component_updater/component_updater_paths.h"

namespace {

constexpr base::FilePath::CharType kDesktopScreenshotEditorBinaryPbFileName[] =
    FILE_PATH_LITERAL("image_editor_app");

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: kdbdaidmledpgkihpopchgmjikgkjclh
constexpr uint8_t kDesktopScreenshotEditorPublicKeySHA256[32] = {
    0x82, 0x30, 0x22, 0x02, 0x0d, 0x30, 0x09, 0x06, 0x86, 0x2a, 0x86,
    0x48, 0x0d, 0xf7, 0x01, 0x01, 0x05, 0x01, 0x03, 0x00, 0x02, 0x82,
    0x00, 0x0f, 0x82, 0x30, 0x0a, 0x02, 0x82, 0x02, 0x01, 0x02};

const char kDesktopScreenshotEditorManifestName[] =
    "Desktop Screenshot Editor App";

}  // namespace

namespace component_updater {

bool DesktopScreenshotEditorComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool DesktopScreenshotEditorComponentInstallerPolicy::
    RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
DesktopScreenshotEditorComponentInstallerPolicy::OnCustomInstall(
    const base::Value& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom added.
}

void DesktopScreenshotEditorComponentInstallerPolicy::OnCustomUninstall() {}

base::FilePath
DesktopScreenshotEditorComponentInstallerPolicy::GetInstalledPath(
    const base::FilePath& base) {
  return base.Append(kDesktopScreenshotEditorBinaryPbFileName);
}

void DesktopScreenshotEditorComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();
}

// Called during startup and installation before ComponentReady().
bool DesktopScreenshotEditorComponentInstallerPolicy::VerifyInstallation(
    const base::Value& manifest,
    const base::FilePath& install_dir) const {
  // TODO(skare): We could enforce some sanity checks that files are present.
  // Otherwise, this is a bundle of files that web contents load.
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath
DesktopScreenshotEditorComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("DesktopScreenshotEditor"));
}

void DesktopScreenshotEditorComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kDesktopScreenshotEditorPublicKeySHA256),
               std::end(kDesktopScreenshotEditorPublicKeySHA256));
}

std::string DesktopScreenshotEditorComponentInstallerPolicy::GetName() const {
  return kDesktopScreenshotEditorManifestName;
}

update_client::InstallerAttributes
DesktopScreenshotEditorComponentInstallerPolicy::GetInstallerAttributes()
    const {
  return update_client::InstallerAttributes();
}

void RegisterDesktopScreenshotEditorComponent(
    component_updater::ComponentUpdateService* cus) {
  // Require either Upcoming Sharing Features or DesktopScreenshotsEdit.
  if (!share::AreUpcomingSharingFeaturesEnabled() &&
      !base::FeatureList::IsEnabled(share::kSharingDesktopScreenshotsEdit)) {
    return;
  }
  VLOG(1) << "Registering Screenshot Editor component.";
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<DesktopScreenshotEditorComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
