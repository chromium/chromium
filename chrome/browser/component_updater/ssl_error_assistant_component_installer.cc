// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/ssl_error_assistant_component_installer.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/ssl/ssl_error_assistant.h"
#include "chrome/browser/ssl/ssl_error_handler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using component_updater::ComponentUpdateService;

namespace {

const base::FilePath::CharType kConfigBinaryPbFileName[] =
    FILE_PATH_LITERAL("ssl_error_assistant.pb");

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: giekcmmlnklenlaomppkphknjmnnpneh
const uint8_t kSslErrorAssistantPublicKeySHA256[32] = {
    0x68, 0x4a, 0x2c, 0xcb, 0xda, 0xb4, 0xdb, 0x0e, 0xcf, 0xfa, 0xf7,
    0xad, 0x9c, 0xdd, 0xfd, 0x47, 0x97, 0xe4, 0x73, 0x24, 0x67, 0x93,
    0x9c, 0xb1, 0x14, 0xcd, 0x3f, 0x54, 0x66, 0x25, 0x99, 0x3f};

void LoadProtoFromDisk(const base::FilePath& pb_path) {
  if (pb_path.empty())
    return;

  std::string binary_pb;
  if (!base::ReadFileToString(pb_path, &binary_pb)) {
    // The file won't exist on new installations, so this is not always an
    // error.
    DVLOG(1) << "Failed reading from " << pb_path.value();
    return;
  }
  auto proto = std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  if (!proto->ParseFromString(binary_pb)) {
    DVLOG(1) << "Failed parsing proto " << pb_path.value();
    return;
  }

  // Retrieve the default proto from the resource bundle and keep the most
  // recent version. This is required since the component updater may still have
  // an older version.
  std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> default_proto =
      SSLErrorAssistant::GetErrorAssistantProtoFromResourceBundle();
  if (default_proto && default_proto->version_id() > proto->version_id()) {
    proto = std::move(default_proto);
  }

  base::CreateSingleThreadTaskRunner({content::BrowserThread::UI})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&SSLErrorHandler::SetErrorAssistantProto,
                                std::move(proto)));
}

}  // namespace

namespace component_updater {

bool SSLErrorAssistantComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

bool SSLErrorAssistantComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
SSLErrorAssistantComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void SSLErrorAssistantComponentInstallerPolicy::OnCustomUninstall() {}

base::FilePath SSLErrorAssistantComponentInstallerPolicy::GetInstalledPath(
    const base::FilePath& base) {
  return base.Append(kConfigBinaryPbFileName);
}

void SSLErrorAssistantComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  DVLOG(1) << "Component ready, version " << version.GetString() << " in "
           << install_dir.value();

  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadProtoFromDisk, GetInstalledPath(install_dir)));
}

// Called during startup and installation before ComponentReady().
bool SSLErrorAssistantComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  // No need to actually validate the proto here, since we'll do the checking
  // in PopulateFromDynamicUpdate().
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath
SSLErrorAssistantComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("SSLErrorAssistant"));
}

void SSLErrorAssistantComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(kSslErrorAssistantPublicKeySHA256,
               kSslErrorAssistantPublicKeySHA256 +
                   base::size(kSslErrorAssistantPublicKeySHA256));
}

std::string SSLErrorAssistantComponentInstallerPolicy::GetName() const {
  // This is a user visible string, so using something other than SSL and TLS.
  return "Certificate Error Assistant";
}

update_client::InstallerAttributes
SSLErrorAssistantComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

std::vector<std::string>
SSLErrorAssistantComponentInstallerPolicy::GetMimeTypes() const {
  return std::vector<std::string>();
}

void RegisterSSLErrorAssistantComponent(ComponentUpdateService* cus,
                                        const base::FilePath& user_data_dir) {
  DVLOG(1) << "Registering SSL Error Assistant component.";

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<SSLErrorAssistantComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
