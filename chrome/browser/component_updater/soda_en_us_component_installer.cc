// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/soda_en_us_component_installer.h"

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
// The component id is: oegebmmcimckjhkhbggblnkjloogjdfg
constexpr uint8_t kSodaEnUsPublicKeySHA256[32] = {
    0xe4, 0x64, 0x1c, 0xc2, 0x8c, 0x2a, 0x97, 0xa7, 0x16, 0x61, 0xbd,
    0xa9, 0xbe, 0xe6, 0x93, 0x56, 0xf5, 0x05, 0x33, 0x9b, 0x8b, 0x0b,
    0x02, 0xe2, 0x6b, 0x7e, 0x6c, 0x40, 0xa1, 0xd2, 0x7e, 0x18};

static_assert(base::size(kSodaEnUsPublicKeySHA256) == crypto::kSHA256Length,
              "Wrong hash length");

constexpr char kSodaEnUsManifestName[] = "SODA en-US Models";

}  // namespace

SodaEnUsComponentInstallerPolicy::SodaEnUsComponentInstallerPolicy(
    OnSodaEnUsComponentReadyCallback callback)
    : on_component_ready_callback_(callback) {}

SodaEnUsComponentInstallerPolicy::~SodaEnUsComponentInstallerPolicy() = default;

const std::string SodaEnUsComponentInstallerPolicy::GetExtensionId() {
  return crx_file::id_util::GenerateIdFromHash(
      kSodaEnUsPublicKeySHA256, sizeof(kSodaEnUsPublicKeySHA256));
}

void SodaEnUsComponentInstallerPolicy::UpdateSodaEnUsComponentOnDemand() {
  const std::string crx_id =
      component_updater::SodaEnUsComponentInstallerPolicy::GetExtensionId();
  g_browser_process->component_updater()->GetOnDemandUpdater().OnDemandUpdate(
      crx_id, component_updater::OnDemandUpdater::Priority::FOREGROUND,
      base::BindOnce([](update_client::Error error) {
        if (error != update_client::Error::NONE &&
            error != update_client::Error::UPDATE_IN_PROGRESS)
          LOG(ERROR) << "On demand update of the SODA en-US component failed "
                        "with error: "
                     << static_cast<int>(error);
      }));
}

bool SodaEnUsComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(
      install_dir.Append(speech::kSodaLanguagePackDirectoryRelativePath));
}

bool SodaEnUsComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool SodaEnUsComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return true;
}

update_client::CrxInstaller::Result
SodaEnUsComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void SodaEnUsComponentInstallerPolicy::OnCustomUninstall() {}

void SodaEnUsComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();

  on_component_ready_callback_.Run(install_dir);
}

base::FilePath SodaEnUsComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(speech::kSodaEnUsInstallationRelativePath);
}

void SodaEnUsComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(kSodaEnUsPublicKeySHA256,
               kSodaEnUsPublicKeySHA256 + base::size(kSodaEnUsPublicKeySHA256));
}

std::string SodaEnUsComponentInstallerPolicy::GetName() const {
  return kSodaEnUsManifestName;
}

update_client::InstallerAttributes
SodaEnUsComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

std::vector<std::string> SodaEnUsComponentInstallerPolicy::GetMimeTypes()
    const {
  return std::vector<std::string>();
}

void UpdateSodaEnUsInstallDirPref(PrefService* prefs,
                                  const base::FilePath& install_dir) {
#if !defined(OS_ANDROID)
  prefs->SetFilePath(
      prefs::kSodaEnUsConfigPath,
      install_dir.Append(speech::kSodaLanguagePackDirectoryRelativePath));
#endif
}

void RegisterSodaEnUsComponent(ComponentUpdateService* cus,
                               PrefService* prefs,
                               base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption)) {
    if (!prefs->GetBoolean(prefs::kLiveCaptionEnabled) ||
        !base::FeatureList::IsEnabled(media::kLiveCaption)) {
      return;
    }

    auto installer = base::MakeRefCounted<ComponentInstaller>(
        std::make_unique<SodaEnUsComponentInstallerPolicy>(base::BindRepeating(
            [](ComponentUpdateService* cus, PrefService* prefs,
               const base::FilePath& install_dir) {
              content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
                  ->PostTask(FROM_HERE,
                             base::BindOnce(&UpdateSodaEnUsInstallDirPref,
                                            prefs, install_dir));
            },
            cus, prefs)));

    installer->Register(cus, std::move(callback));
  }
}

bool UninstallSodaEnUsComponent(ComponentUpdateService* cus,
                                PrefService* prefs) {
  return cus->UnregisterComponent(
      SodaEnUsComponentInstallerPolicy::GetExtensionId());
}
}  // namespace component_updater
