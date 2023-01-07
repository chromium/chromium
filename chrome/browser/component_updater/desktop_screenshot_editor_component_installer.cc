// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/desktop_screenshot_editor_component_installer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "chrome/browser/image_editor/image_editor_component_info.h"
#include "chrome/browser/share/share_features.h"
#include "components/component_updater/component_updater_paths.h"

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: kdbdaidmledpgkihpopchgmjikgkjclh
constexpr uint8_t kDesktopScreenshotEditorPublicKeySHA256[32] = {
    0xa3, 0x13, 0x08, 0x3c, 0xb4, 0x3f, 0x6a, 0x87, 0xfe, 0xf2, 0x76,
    0xc9, 0x8a, 0x6a, 0x92, 0xb7, 0xf5, 0xd5, 0x97, 0x31, 0x3f, 0x07,
    0x7b, 0x87, 0xd0, 0x3c, 0x5c, 0xc2, 0x1b, 0x7a, 0xac, 0x12};

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
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom added.
}

void DesktopScreenshotEditorComponentInstallerPolicy::OnCustomUninstall() {}

void DesktopScreenshotEditorComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();
  image_editor::ImageEditorComponentInfo::GetInstance()->SetInstalledPath(
      install_dir);
}

// Called during startup and installation before ComponentReady().
bool DesktopScreenshotEditorComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  // The component installs as a zip of resources.
  // We check for an index.html which should always be present.
  // TODO(skare): Extend once we have cross-project library names determined.
  base::FilePath index_path = install_dir.AppendASCII("index.html");
  return base::PathExists(index_path);
}

base::FilePath
DesktopScreenshotEditorComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("ScreenshotImageEditor"));
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
