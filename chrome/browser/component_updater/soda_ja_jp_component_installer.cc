// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/soda_ja_jp_component_installer.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/soda/constants.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "media/base/media_switches.h"

using content::BrowserThread;

namespace component_updater {

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The component id is: onhpjgkfgajmkkeniaoflicgokpaebfa
constexpr uint8_t kSodaJaJpPublicKeySHA256[32] = {
    0xed, 0x7f, 0x96, 0xa5, 0x60, 0x9c, 0xaa, 0x4d, 0x80, 0xe5, 0xb8,
    0x26, 0xea, 0xf0, 0x41, 0x50, 0x09, 0x52, 0xa4, 0xb3, 0x1e, 0x6a,
    0x8e, 0x24, 0x99, 0xde, 0x51, 0x14, 0xc4, 0x3c, 0xfa, 0x48};

static_assert(base::size(kSodaJaJpPublicKeySHA256) == crypto::kSHA256Length,
              "Wrong hash length");

constexpr char kSodaJaJpManifestName[] = "SODA ja-JP Models";

}  // namespace

SodaJaJpComponentInstallerPolicy::SodaJaJpComponentInstallerPolicy(
    OnSodaJaJpComponentReadyCallback callback)
    : on_component_ready_callback_(callback) {}

SodaJaJpComponentInstallerPolicy::~SodaJaJpComponentInstallerPolicy() = default;

const std::string SodaJaJpComponentInstallerPolicy::GetExtensionId() {
  return crx_file::id_util::GenerateIdFromHash(
      kSodaJaJpPublicKeySHA256, sizeof(kSodaJaJpPublicKeySHA256));
}

void SodaJaJpComponentInstallerPolicy::UpdateSodaJaJpComponentOnDemand() {
  const std::string crx_id =
      component_updater::SodaJaJpComponentInstallerPolicy::GetExtensionId();
  g_browser_process->component_updater()->GetOnDemandUpdater().OnDemandUpdate(
      crx_id, component_updater::OnDemandUpdater::Priority::FOREGROUND,
      base::BindOnce([](update_client::Error error) {
        if (error != update_client::Error::NONE &&
            error != update_client::Error::UPDATE_IN_PROGRESS)
          LOG(ERROR) << "On demand update of the SODA ja-JP component failed "
                        "with error: "
                     << static_cast<int>(error);
      }));
}

bool SodaJaJpComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(
      install_dir.Append(speech::kSodaLanguagePackDirectoryRelativePath));
}

bool SodaJaJpComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool SodaJaJpComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return true;
}

update_client::CrxInstaller::Result
SodaJaJpComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void SodaJaJpComponentInstallerPolicy::OnCustomUninstall() {}

void SodaJaJpComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();

  on_component_ready_callback_.Run(install_dir);
}

base::FilePath SodaJaJpComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(speech::kSodaJaJpInstallationRelativePath);
}

void SodaJaJpComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(kSodaJaJpPublicKeySHA256,
               kSodaJaJpPublicKeySHA256 + base::size(kSodaJaJpPublicKeySHA256));
}

std::string SodaJaJpComponentInstallerPolicy::GetName() const {
  return kSodaJaJpManifestName;
}

update_client::InstallerAttributes
SodaJaJpComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

std::vector<std::string> SodaJaJpComponentInstallerPolicy::GetMimeTypes()
    const {
  return std::vector<std::string>();
}

void UpdateSodaJaJpInstallDirPref(PrefService* prefs,
                                  const base::FilePath& install_dir) {
#if !defined(OS_ANDROID)
  prefs->SetFilePath(
      prefs::kSodaJaJpConfigPath,
      install_dir.Append(speech::kSodaLanguagePackDirectoryRelativePath));
#endif
}

void RegisterSodaJaJpComponent(ComponentUpdateService* cus,
                               PrefService* prefs,
                               base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption)) {
    if (!prefs->GetBoolean(prefs::kLiveCaptionEnabled) ||
        !base::FeatureList::IsEnabled(media::kLiveCaption)) {
      return;
    }

    auto installer = base::MakeRefCounted<ComponentInstaller>(
        std::make_unique<SodaJaJpComponentInstallerPolicy>(base::BindRepeating(
            [](ComponentUpdateService* cus, PrefService* prefs,
               const base::FilePath& install_dir) {
              content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
                  ->PostTask(FROM_HERE,
                             base::BindOnce(&UpdateSodaJaJpInstallDirPref,
                                            prefs, install_dir));
            },
            cus, prefs)));

    installer->Register(cus, std::move(callback));
  }
}

bool UninstallSodaJaJpComponent(ComponentUpdateService* cus,
                                PrefService* prefs) {
  return cus->UnregisterComponent(
      SodaJaJpComponentInstallerPolicy::GetExtensionId());
}
}  // namespace component_updater
