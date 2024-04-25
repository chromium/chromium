// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/crl_set_component_installer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "net/cert/crl_set.h"
#include "net/ssl/ssl_config_service.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"

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
//
// TODO(crbug.com/40693524): if CertVerifierServiceFactory is moved out
// of the browser process, this will need to be updated to handle
// CertVerifierServiceFactory disconnections/restarts, so that a newly
// restarted CertVerifierServiceFactory can be reinitialized with the current
// CRLSet component data. (There used to be code to handle network service
// restarts here, so you could look in the revision history for that as a
// hint.)
class CRLSetData {
 public:
  // Sets the latest CRLSet to |crl_set_path|. Call
  // |ConfigureCertVerifierServiceFactory()| to actually load that path.
  void set_crl_set_path(const base::FilePath& crl_set_path) {
    crl_set_path_ = crl_set_path;
  }

  // Updates the CertVerifierServiceFactory with the current CRLSet
  // configuration.
  void ConfigureCertVerifierServiceFactory();

 private:
  void UpdateCRLSetOnUI(const std::string& crl_set_bytes);

  base::FilePath crl_set_path_;
};

base::LazyInstance<CRLSetData>::Leaky g_crl_set_data =
    LAZY_INSTANCE_INITIALIZER;

void CRLSetData::ConfigureCertVerifierServiceFactory() {
  if (crl_set_path_.empty()) {
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&LoadCRLSet, crl_set_path_),
      base::BindOnce(&CRLSetData::UpdateCRLSetOnUI, base::Unretained(this)));
}

void CRLSetData::UpdateCRLSetOnUI(const std::string& crl_set_bytes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::GetCertVerifierServiceFactory()->UpdateCRLSet(
      base::as_bytes(base::make_span(crl_set_bytes)), base::DoNothing());
}

}  // namespace

CRLSetPolicy::CRLSetPolicy() = default;
CRLSetPolicy::~CRLSetPolicy() = default;

bool CRLSetPolicy::VerifyInstallation(const base::Value::Dict& manifest,
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
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void CRLSetPolicy::OnCustomUninstall() {}

void CRLSetPolicy::ComponentReady(const base::Version& version,
                                  const base::FilePath& install_dir,
                                  base::Value::Dict manifest) {
  g_crl_set_data.Get().set_crl_set_path(install_dir.Append(kCRLSetFile));
  g_crl_set_data.Get().ConfigureCertVerifierServiceFactory();
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

void RegisterCRLSetComponent(ComponentUpdateService* cus) {
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<CRLSetPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
