// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/client_side_phishing_component_installer.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/safe_browsing/content/browser/client_side_phishing_model.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/variations/variations_associated_data.h"

using component_updater::ComponentUpdateService;

namespace {

const base::FilePath::CharType kClientModelBinaryPbFileName[] =
    FILE_PATH_LITERAL("client_model.pb");
const base::FilePath::CharType kVisualTfLiteModelFileName[] =
    FILE_PATH_LITERAL("visual_model.tflite");

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: imefjhfbkmcmebodilednhmaccmincoa
const uint8_t kClientSidePhishingPublicKeySHA256[32] = {
    0x8c, 0x45, 0x97, 0x51, 0xac, 0x2c, 0x41, 0xe3, 0x8b, 0x43, 0xd7,
    0xc0, 0x22, 0xc8, 0xd2, 0xe0, 0xe3, 0xe2, 0x33, 0x88, 0x1f, 0x09,
    0x6d, 0xde, 0x65, 0x6a, 0x83, 0x32, 0x71, 0x52, 0x6e, 0x77};

const char kClientSidePhishingManifestName[] = "Client Side Phishing Detection";

void LoadFromDisk(const base::FilePath& pb_path,
                  const base::FilePath& visual_tflite_model_path) {
  if (pb_path.empty())
    return;

  std::string binary_pb;
  if (!base::ReadFileToString(pb_path, &binary_pb))
    binary_pb.clear();

  base::File visual_tflite_model(visual_tflite_model_path,
                                 base::File::FLAG_OPEN | base::File::FLAG_READ);

  // The ClientSidePhishingModel singleton will react appropriately if the
  // |binary_pb| is empty or |visual_tflite_model| is invalid.
  safe_browsing::ClientSidePhishingModel::GetInstance()
      ->PopulateFromDynamicUpdate(binary_pb, std::move(visual_tflite_model));
}

base::FilePath GetInstalledProtoPath(const base::FilePath& base) {
  return base.Append(kClientModelBinaryPbFileName);
}

base::FilePath GetInstalledTfLitePath(const base::FilePath& base) {
  return base.Append(kVisualTfLiteModelFileName);
}

}  // namespace

namespace component_updater {

bool ClientSidePhishingComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

bool ClientSidePhishingComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
ClientSidePhishingComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void ClientSidePhishingComponentInstallerPolicy::OnCustomUninstall() {}

void ClientSidePhishingComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadFromDisk, GetInstalledProtoPath(install_dir),
                     GetInstalledTfLitePath(install_dir)));
}

// Called during startup and installation before ComponentReady().
bool ClientSidePhishingComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  // No need to actually validate the proto here, since we'll do the checking
  // in PopulateFromDynamicUpdate().
  return base::PathExists(GetInstalledProtoPath(install_dir)) ||
         base::PathExists(GetInstalledTfLitePath(install_dir));
}

base::FilePath
ClientSidePhishingComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("ClientSidePhishing"));
}

void ClientSidePhishingComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(kClientSidePhishingPublicKeySHA256,
               kClientSidePhishingPublicKeySHA256 +
                   base::size(kClientSidePhishingPublicKeySHA256));
}

std::string ClientSidePhishingComponentInstallerPolicy::GetName() const {
  return kClientSidePhishingManifestName;
}

update_client::InstallerAttributes
ClientSidePhishingComponentInstallerPolicy::GetInstallerAttributes() const {
  update_client::InstallerAttributes attributes;

  // Pass the tag parameter to the installer as the "tag" attribute; it will
  // be used to choose which binary is downloaded.
  attributes["tag"] = safe_browsing::GetClientSideDetectionTag();
  return attributes;
}

void RegisterClientSidePhishingComponent(ComponentUpdateService* cus) {
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<ClientSidePhishingComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
