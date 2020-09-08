// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/tls_deprecation_config_component_installer.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/ssl/tls_deprecation_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/mojom/network_service.mojom.h"

using component_updater::ComponentUpdateService;

namespace component_updater {

namespace {

const base::FilePath::CharType kTLSDeprecationConfigBinaryPbFileName[] =
    FILE_PATH_LITERAL("tls_deprecation_config.pb");

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: bklopemakmnopmghhmccadeonafabnal
const uint8_t kTLSDeprecationConfigPublicKeySHA256[32] = {
    0x1a, 0xbe, 0xf4, 0xc0, 0xac, 0xde, 0xfc, 0x67, 0x7c, 0x22, 0x03,
    0x4e, 0xd0, 0x50, 0x1d, 0x0b, 0xed, 0x45, 0x0f, 0xcb, 0x0b, 0x7f,
    0xad, 0x4f, 0xb6, 0x7b, 0x7c, 0x8f, 0xbf, 0xda, 0xa8, 0xe3};

// Singleton object used to configure Network Services and memoize the TLS
// deprecation configuration, so that it can be reloaded when the Network
// Service restarts.
base::FilePath& GetConfigPathInstance() {
  static base::NoDestructor<base::FilePath> instance;
  return *instance;
}

base::FilePath GetInstalledPath(const base::FilePath& base) {
  return base.Append(kTLSDeprecationConfigBinaryPbFileName);
}

// Returns the contents of the file at |config_path|.
std::string LoadConfig(const base::FilePath& config_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  std::string config_bytes;
  base::ReadFileToString(config_path, &config_bytes);
  return config_bytes;
}

// Updates the browser and network service with a new binary config.
void UpdateLegacyTLSConfigOnUI(const std::string& binary_config) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  SetRemoteTLSDeprecationConfig(binary_config);

  network::mojom::NetworkService* network_service =
      content::GetNetworkService();
  network_service->UpdateLegacyTLSConfig(
      base::as_bytes(base::make_span(binary_config)), base::DoNothing());
}

void SetLegacyTLSConfig() {
  if (GetConfigPathInstance().empty())
    return;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&LoadConfig, GetConfigPathInstance()),
      base::BindOnce(&UpdateLegacyTLSConfigOnUI));
}

}  // namespace

TLSDeprecationConfigComponentInstallerPolicy::
    TLSDeprecationConfigComponentInstallerPolicy() = default;

TLSDeprecationConfigComponentInstallerPolicy::
    ~TLSDeprecationConfigComponentInstallerPolicy() = default;

// static
void TLSDeprecationConfigComponentInstallerPolicy::
    ReconfigureAfterNetworkRestart() {
  SetLegacyTLSConfig();
}

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

  // Save the installed component path for future reloads.
  GetConfigPathInstance() = pb_path;

  // The default proto will always be a placeholder since the updated versions
  // are not checked into the repo. Simply load whatever the component updater
  // gave us without checking the default proto from the resource bundle.
  SetLegacyTLSConfig();
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

void RegisterTLSDeprecationConfigComponent(ComponentUpdateService* cus) {
  DVLOG(1) << "Registering TLS Deprecation Config component.";

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<TLSDeprecationConfigComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
