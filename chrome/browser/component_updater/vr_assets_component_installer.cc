// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/vr_assets_component_installer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/vr/assets_loader.h"
#include "chrome/browser/vr/metrics/metrics_helper.h"
#include "chrome/browser/vr/vr_buildflags.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/VrAssetsComponentInstaller_jni.h"
#endif  // defined(OS_ANDROID)

using component_updater::ComponentUpdateService;

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: cjfkbpdpjpdldhclahpfgnlhpodlpnba
const uint8_t kVrAssetsPublicKeySHA256[32] = {
    0x29, 0x5a, 0x1f, 0x3f, 0x9f, 0x3b, 0x37, 0x2b, 0x07, 0xf5, 0x6d,
    0xb7, 0xfe, 0x3b, 0xfd, 0x10, 0xb6, 0x80, 0xf3, 0x66, 0x0d, 0xc3,
    0xe2, 0x07, 0x25, 0x8d, 0x37, 0x85, 0x39, 0x51, 0x58, 0xcf};

const char kVrAssetsComponentName[] = "VR Assets";
const base::FilePath::CharType kRelativeInstallDir[] =
    FILE_PATH_LITERAL("VrAssets");

}  // namespace

namespace component_updater {

bool VrAssetsComponentInstallerPolicy::registered_component_ = false;
bool VrAssetsComponentInstallerPolicy::registration_pending_ = false;
bool VrAssetsComponentInstallerPolicy::ondemand_update_pending_ = false;

// static
bool VrAssetsComponentInstallerPolicy::
    ShouldRegisterVrAssetsComponentOnStartup() {
#if defined(OS_ANDROID)
  return Java_VrAssetsComponentInstaller_shouldRegisterOnStartup(
      base::android::AttachCurrentThread());
#else   // defined(OS_ANDROID)
  return false;
#endif  // defined(OS_ANDROID)
}

// static
void VrAssetsComponentInstallerPolicy::RegisterComponent(
    ComponentUpdateService* cus) {
#if BUILDFLAG(USE_VR_ASSETS_COMPONENT)
  if (registration_pending_ || registered_component_) {
    return;
  }
  const std::string crx_id = crx_file::id_util::GenerateIdFromHash(
      kVrAssetsPublicKeySHA256, sizeof(kVrAssetsPublicKeySHA256));
  std::unique_ptr<VrAssetsComponentInstallerPolicy> policy(
      new VrAssetsComponentInstallerPolicy());
  auto installer = base::MakeRefCounted<ComponentInstaller>(std::move(policy));
  registration_pending_ = true;
  installer->Register(
      cus,
      base::BindOnce(&VrAssetsComponentInstallerPolicy::OnRegisteredComponent,
                     base::Unretained(cus)));
#endif  // BUILDFLAG(USE_VR_ASSETS_COMPONENT)
}

// static
void VrAssetsComponentInstallerPolicy::UpdateComponent(
    ComponentUpdateService* cus) {
#if BUILDFLAG(USE_VR_ASSETS_COMPONENT)
  if (registration_pending_) {
    ondemand_update_pending_ = true;
    return;
  }
  ondemand_update_pending_ = false;
  const std::string crx_id = crx_file::id_util::GenerateIdFromHash(
      kVrAssetsPublicKeySHA256, sizeof(kVrAssetsPublicKeySHA256));
  cus->GetOnDemandUpdater().OnDemandUpdate(
      crx_id, OnDemandUpdater::Priority::FOREGROUND,
      base::BindOnce([](update_client::Error error) { return; }));
#endif  // BUILDFLAG(USE_VR_ASSETS_COMPONENT)
}

// static
void VrAssetsComponentInstallerPolicy::OnRegisteredComponent(
    ComponentUpdateService* cus) {
  vr::AssetsLoader::GetInstance()->GetMetricsHelper()->OnRegisteredComponent();
  VLOG(1) << "Registered VR assets component";
  registration_pending_ = false;
  registered_component_ = true;
  if (ondemand_update_pending_) {
    UpdateComponent(cus);
  }
}

bool VrAssetsComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

bool VrAssetsComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
VrAssetsComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);
}

void VrAssetsComponentInstallerPolicy::OnCustomUninstall() {}

// Called during startup and installation before ComponentReady().
bool VrAssetsComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  auto* version_value = manifest.FindKey("version");
  if (!version_value || !version_value->is_string()) {
    vr::AssetsLoader::GetInstance()->GetMetricsHelper()->OnComponentUpdated(
        vr::AssetsComponentUpdateStatus::kInvalid, base::nullopt);
    return false;
  }

  auto version_string = version_value->GetString();
  base::Version version(version_string);
  if (!version.IsValid() || version.components().size() != 2 ||
      !base::PathExists(install_dir)) {
    vr::AssetsLoader::GetInstance()->GetMetricsHelper()->OnComponentUpdated(
        vr::AssetsComponentUpdateStatus::kInvalid, base::nullopt);
    return false;
  }

  if (version.components()[0] > vr::kTargetMajorVrAssetsComponentVersion) {
    // Component needs to be downgraded. Differential downgrades are not
    // supported. Just delete this component version.
    vr::AssetsLoader::GetInstance()->GetMetricsHelper()->OnComponentUpdated(
        vr::AssetsComponentUpdateStatus::kIncompatible, version);
    return false;
  }

  return true;
}

void VrAssetsComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  if (version.components()[0] < vr::kMinMajorVrAssetsComponentVersion) {
    // Don't propagate component readiness and wait until differential update
    // delivers compatible component version.
    vr::AssetsLoader::GetInstance()->GetMetricsHelper()->OnComponentUpdated(
        vr::AssetsComponentUpdateStatus::kIncompatible, version);
    return;
  }
  vr::AssetsLoader::GetInstance()->OnComponentReady(version, install_dir,
                                                    std::move(manifest));
}

base::FilePath VrAssetsComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(kRelativeInstallDir);
}

void VrAssetsComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(kVrAssetsPublicKeySHA256,
               kVrAssetsPublicKeySHA256 + base::size(kVrAssetsPublicKeySHA256));
}

std::string VrAssetsComponentInstallerPolicy::GetName() const {
  return kVrAssetsComponentName;
}

update_client::InstallerAttributes
VrAssetsComponentInstallerPolicy::GetInstallerAttributes() const {
  return {{"compatible_major_version",
           std::to_string(vr::kTargetMajorVrAssetsComponentVersion)}};
}

std::vector<std::string> VrAssetsComponentInstallerPolicy::GetMimeTypes()
    const {
  return std::vector<std::string>();
}

bool ShouldRegisterVrAssetsComponentOnStartup() {
  return VrAssetsComponentInstallerPolicy::
      ShouldRegisterVrAssetsComponentOnStartup();
}

void RegisterVrAssetsComponent(ComponentUpdateService* cus) {
  VrAssetsComponentInstallerPolicy::RegisterComponent(cus);
}

void UpdateVrAssetsComponent(ComponentUpdateService* cus) {
  VrAssetsComponentInstallerPolicy::UpdateComponent(cus);
}

}  // namespace component_updater
