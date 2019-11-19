// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/tls_deprecation_config_component_installer.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/ssl/tls_deprecation_config.h"
#include "chrome/browser/ssl/tls_deprecation_config.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using component_updater::ComponentUpdateService;

namespace {

const base::FilePath::CharType kTLSDeprecationConfigBinaryPbFileName[] =
    FILE_PATH_LITERAL("tls_deprecation_config.pb");

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: bklopemakmnopmghhmccadeonafabnal
const uint8_t kTLSDeprecationConfigPublicKeySHA256[32] = {
    0x1a, 0xbe, 0xf4, 0xc0, 0xac, 0xde, 0xfc, 0x67, 0x7c, 0x22, 0x03,
    0x4e, 0xd0, 0x50, 0x1d, 0x0b, 0xed, 0x45, 0x0f, 0xcb, 0x0b, 0x7f,
    0xad, 0x4f, 0xb6, 0x7b, 0x7c, 0x8f, 0xbf, 0xda, 0xa8, 0xe3};

std::unique_ptr<chrome_browser_ssl::LegacyTLSExperimentConfig>
LoadTLSDeprecationConfigProtoFromDisk(const base::FilePath& pb_path) {
  std::string binary_pb;
  if (!base::ReadFileToString(pb_path, &binary_pb)) {
    // The file won't exist on new installations, so this is not always an
    // error.
    DVLOG(1) << "Failed reading from " << pb_path.value();
    return nullptr;
  }
  auto proto =
      std::make_unique<chrome_browser_ssl::LegacyTLSExperimentConfig>();
  if (!proto->ParseFromString(binary_pb)) {
    DVLOG(1) << "Failed parsing proto " << pb_path.value();
    return nullptr;
  }
  return proto;
}

base::FilePath GetInstalledPath(const base::FilePath& base) {
  return base.Append(kTLSDeprecationConfigBinaryPbFileName);
}

}  // namespace

namespace component_updater {

TLSDeprecationConfigComponentInstallerPolicy::
    TLSDeprecationConfigComponentInstallerPolicy() = default;

TLSDeprecationConfigComponentInstallerPolicy::
    ~TLSDeprecationConfigComponentInstallerPolicy() = default;

bool TLSDeprecationConfigComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

bool TLSDeprecationConfigComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
TLSDeprecationConfigComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& /* manifest */,
    const base::FilePath& /* install_dir */) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void TLSDeprecationConfigComponentInstallerPolicy::OnCustomUninstall() {}

void TLSDeprecationConfigComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << "Component ready, version " << version.GetString() << " in "
           << install_dir.value();

  const base::FilePath pb_path = GetInstalledPath(install_dir);
  if (pb_path.empty())
    return;

  // The default proto will always be a placeholder since the updated versions
  // are not checked into the repo. Simply load whatever the component updater
  // gave us without checking the default proto from the resource bundle.
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&LoadTLSDeprecationConfigProtoFromDisk, pb_path),
      base::BindOnce(&SetRemoteTLSDeprecationConfigProto));
}

// Called during startup and installation before ComponentReady().
bool TLSDeprecationConfigComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& /* manifest */,
    const base::FilePath& install_dir) const {
  // No need to actually validate the proto here, since we'll do the checking
  // in PopulateFromDynamicUpdate().
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath
TLSDeprecationConfigComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("TLSDeprecationConfig"));
}

void TLSDeprecationConfigComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(kTLSDeprecationConfigPublicKeySHA256,
               kTLSDeprecationConfigPublicKeySHA256 +
                   base::size(kTLSDeprecationConfigPublicKeySHA256));
}

std::string TLSDeprecationConfigComponentInstallerPolicy::GetName() const {
  return "Legacy TLS Deprecation Configuration";
}

update_client::InstallerAttributes
TLSDeprecationConfigComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

std::vector<std::string>
TLSDeprecationConfigComponentInstallerPolicy::GetMimeTypes() const {
  return std::vector<std::string>();
}

void RegisterTLSDeprecationConfigComponent(
    ComponentUpdateService* cus,
    const base::FilePath& user_data_dir) {
  DVLOG(1) << "Registering TLS Deprecation Config component.";

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<TLSDeprecationConfigComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
