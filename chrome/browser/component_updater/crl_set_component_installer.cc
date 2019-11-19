// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/crl_set_component_installer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "net/cert/crl_set.h"
#include "net/ssl/ssl_config_service.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace component_updater {

namespace {

// kCrlSetPublicKeySHA256 is the SHA256 hash of the SubjectPublicKeyInfo of the
// key that's used to sign generated CRL sets.
const uint8_t kCrlSetPublicKeySHA256[32] = {
    0x75, 0xda, 0xf8, 0xcb, 0x77, 0x68, 0x40, 0x33, 0x65, 0x4c, 0x97,
    0xe5, 0xc5, 0x1b, 0xcd, 0x81, 0x7b, 0x1e, 0xeb, 0x11, 0x2c, 0xe1,
    0xa4, 0x33, 0x8c, 0xf5, 0x72, 0x5e, 0xed, 0xb8, 0x43, 0x97,
};

const base::FilePath::CharType kCRLSetFile[] = FILE_PATH_LITERAL("crl-set");

// Returns the contents of the file at |crl_path|.
std::string LoadCRLSet(const base::FilePath& crl_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  std::string crl_set_bytes;
  base::ReadFileToString(crl_path, &crl_set_bytes);
  return crl_set_bytes;
}

// Singleton object used to configure Network Services and memoize the CRLSet
// configuration.
class CRLSetData {
 public:
  // Sets the latest CRLSet to |crl_set_path|. Call
  // |ConfigureNetworkService()| to actually load that path.
  void set_crl_set_path(const base::FilePath& crl_set_path) {
    crl_set_path_ = crl_set_path;
  }

  // Configures updates to be sent to |network_service|, rather than
  // content::GetNetworkService(). This should only be used for tests.
  void set_network_service(network::mojom::NetworkService* network_service) {
    network_service_ = network_service;
  }

  // Updates the currently configured network service (or
  // content::GetNetworkService()) with the current CRLSet configuration.
  void ConfigureNetworkService();

 private:
  void UpdateCRLSetOnUI(const std::string& crl_set_bytes);

  network::mojom::NetworkService* network_service_ = nullptr;
  base::FilePath crl_set_path_;
};

base::LazyInstance<CRLSetData>::Leaky g_crl_set_data =
    LAZY_INSTANCE_INITIALIZER;

void CRLSetData::ConfigureNetworkService() {
  if (crl_set_path_.empty())
    return;

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&LoadCRLSet, crl_set_path_),
      base::BindOnce(&CRLSetData::UpdateCRLSetOnUI, base::Unretained(this)));
}

void CRLSetData::UpdateCRLSetOnUI(const std::string& crl_set_bytes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  network::mojom::NetworkService* network_service =
      network_service_ ? network_service_ : content::GetNetworkService();
  network_service->UpdateCRLSet(base::as_bytes(base::make_span(crl_set_bytes)));
}

}  // namespace

CRLSetPolicy::CRLSetPolicy() = default;
CRLSetPolicy::~CRLSetPolicy() = default;

// static
void CRLSetPolicy::SetNetworkServiceForTesting(
    network::mojom::NetworkService* network_service) {
  g_crl_set_data.Get().set_network_service(network_service);
}

// static
void CRLSetPolicy::ReconfigureAfterNetworkRestart() {
  g_crl_set_data.Get().ConfigureNetworkService();
}

bool CRLSetPolicy::VerifyInstallation(const base::DictionaryValue& manifest,
                                      const base::FilePath& install_dir) const {
  return base::PathExists(install_dir.Append(kCRLSetFile));
}

bool CRLSetPolicy::SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

bool CRLSetPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result CRLSetPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void CRLSetPolicy::OnCustomUninstall() {}

void CRLSetPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  g_crl_set_data.Get().set_crl_set_path(install_dir.Append(kCRLSetFile));
  g_crl_set_data.Get().ConfigureNetworkService();
}

base::FilePath CRLSetPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("CertificateRevocation"));
}

void CRLSetPolicy::GetHash(std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kCrlSetPublicKeySHA256),
               std::end(kCrlSetPublicKeySHA256));
}

std::string CRLSetPolicy::GetName() const {
  return "CRLSet";
}

update_client::InstallerAttributes CRLSetPolicy::GetInstallerAttributes()
    const {
  return update_client::InstallerAttributes();
}

std::vector<std::string> CRLSetPolicy::GetMimeTypes() const {
  return std::vector<std::string>();
}

void RegisterCRLSetComponent(ComponentUpdateService* cus,
                             const base::FilePath& user_data_dir) {
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<CRLSetPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
