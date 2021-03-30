// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/soda_component_installer.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/soda_language_pack_component_installer.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/soda/constants.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "media/base/media_switches.h"

#if defined(OS_WIN)
#include <aclapi.h>
#include <windows.h>
#include "sandbox/win/src/sid.h"
#endif

using content::BrowserThread;

namespace component_updater {

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The component id is: icnkogojpkfjeajonkmlplionaamopkf
constexpr uint8_t kSodaPublicKeySHA256[32] = {
    0x82, 0xda, 0xe6, 0xe9, 0xfa, 0x59, 0x40, 0x9e, 0xda, 0xcb, 0xfb,
    0x8e, 0xd0, 0x0c, 0xef, 0xa5, 0xc0, 0x97, 0x00, 0x84, 0x1c, 0x21,
    0xa6, 0xae, 0xc8, 0x1b, 0x87, 0xfb, 0x12, 0x27, 0x28, 0xb1};

static_assert(base::size(kSodaPublicKeySHA256) == crypto::kSHA256Length,
              "Wrong hash length");

constexpr char kSodaManifestName[] = "SODA Library";

}  // namespace

SodaComponentInstallerPolicy::SodaComponentInstallerPolicy(
    OnSodaComponentInstalledCallback on_installed_callback,
    OnSodaComponentReadyCallback on_ready_callback)
    : on_installed_callback_(on_installed_callback),
      on_ready_callback_(std::move(on_ready_callback)) {}

SodaComponentInstallerPolicy::~SodaComponentInstallerPolicy() = default;

const std::string SodaComponentInstallerPolicy::GetExtensionId() {
  return crx_file::id_util::GenerateIdFromHash(kSodaPublicKeySHA256,
                                               sizeof(kSodaPublicKeySHA256));
}

void SodaComponentInstallerPolicy::UpdateSodaComponentOnDemand() {
  const std::string crx_id =
      component_updater::SodaComponentInstallerPolicy::GetExtensionId();
  g_browser_process->component_updater()->GetOnDemandUpdater().OnDemandUpdate(
      crx_id, component_updater::OnDemandUpdater::Priority::FOREGROUND,
      base::BindOnce([](update_client::Error error) {
        if (error != update_client::Error::NONE &&
            error != update_client::Error::UPDATE_IN_PROGRESS)
          LOG(ERROR) << "On demand update of the SODA component failed "
                        "with error: "
                     << static_cast<int>(error);
      }));
}

update_client::CrxInstaller::Result
SodaComponentInstallerPolicy::SetComponentDirectoryPermission(
    const base::FilePath& install_dir) {
#if defined(OS_WIN)
  sandbox::Sid users_sid = sandbox::Sid(WinBuiltinUsersSid);

  // Initialize an EXPLICIT_ACCESS structure for an ACE.
  EXPLICIT_ACCESS explicit_access[1] = {};
  explicit_access[0].grfAccessPermissions = GENERIC_READ | GENERIC_EXECUTE;
  explicit_access[0].grfAccessMode = GRANT_ACCESS;
  explicit_access[0].grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
  explicit_access[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
  explicit_access[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
  explicit_access[0].Trustee.ptstrName =
      reinterpret_cast<LPTSTR>(users_sid.GetPSID());

  PACL acl = nullptr;
  if (::SetEntriesInAcl(base::size(explicit_access), explicit_access, nullptr,
                        &acl) != ERROR_SUCCESS) {
    return update_client::CrxInstaller::Result(
        update_client::InstallError::SET_PERMISSIONS_FAILED);
  }

  // Change the security attributes.
  LPWSTR file_name = const_cast<LPWSTR>(install_dir.value().c_str());
  bool success = ::SetNamedSecurityInfo(file_name, SE_FILE_OBJECT,
                                        DACL_SECURITY_INFORMATION, nullptr,
                                        nullptr, acl, nullptr) == ERROR_SUCCESS;
  ::LocalFree(acl);
  if (!success) {
    return update_client::CrxInstaller::Result(
        update_client::InstallError::SET_PERMISSIONS_FAILED);
  }
#endif

  return update_client::CrxInstaller::Result(update_client::InstallError::NONE);
}

bool SodaComponentInstallerPolicy::SupportsGroupPolicyEnabledComponentUpdates()
    const {
  return true;
}

bool SodaComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return true;
}

update_client::CrxInstaller::Result
SodaComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return SodaComponentInstallerPolicy::SetComponentDirectoryPermission(
      install_dir);
}

void SodaComponentInstallerPolicy::OnCustomUninstall() {}

bool SodaComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(install_dir.Append(speech::kSodaBinaryRelativePath));
}

void SodaComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();
  if (on_installed_callback_)
    on_installed_callback_.Run(install_dir);

  if (on_ready_callback_)
    std::move(on_ready_callback_).Run();
}

base::FilePath SodaComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(speech::kSodaInstallationRelativePath);
}

void SodaComponentInstallerPolicy::GetHash(std::vector<uint8_t>* hash) const {
  hash->assign(kSodaPublicKeySHA256,
               kSodaPublicKeySHA256 + base::size(kSodaPublicKeySHA256));
}

std::string SodaComponentInstallerPolicy::GetName() const {
  return kSodaManifestName;
}

update_client::InstallerAttributes
SodaComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void UpdateSodaInstallDirPref(PrefService* prefs,
                              const base::FilePath& install_dir) {
#if !defined(OS_ANDROID)
  prefs->SetFilePath(prefs::kSodaBinaryPath,
                     install_dir.Append(speech::kSodaBinaryRelativePath));
#endif
}

void RegisterPrefsForSodaComponent(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(prefs::kSodaScheduledDeletionTime, base::Time());
  registry->RegisterFilePathPref(prefs::kSodaBinaryPath, base::FilePath());

  // Register language pack config path preferences.
  for (const speech::SodaLanguagePackComponentConfig& config :
       speech::kLanguageComponentConfigs) {
    registry->RegisterFilePathPref(config.config_path_pref, base::FilePath());
  }
}

void RegisterSodaComponent(ComponentUpdateService* cus,
                           PrefService* profile_prefs,
                           PrefService* global_prefs,
                           base::OnceClosure on_ready_callback,
                           base::OnceClosure on_registered_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption) &&
      base::FeatureList::IsEnabled(media::kLiveCaption)) {
    if (profile_prefs->GetBoolean(prefs::kLiveCaptionEnabled)) {
      global_prefs->SetTime(prefs::kSodaScheduledDeletionTime, base::Time());
      auto installer = base::MakeRefCounted<ComponentInstaller>(
          std::make_unique<SodaComponentInstallerPolicy>(
              base::BindRepeating(
                  [](ComponentUpdateService* cus, PrefService* global_prefs,
                     const base::FilePath& install_dir) {
                    content::GetUIThreadTaskRunner(
                        {base::TaskPriority::USER_BLOCKING})
                        ->PostTask(FROM_HERE,
                                   base::BindOnce(&UpdateSodaInstallDirPref,
                                                  global_prefs, install_dir));
                  },
                  cus, global_prefs),
              std::move(on_ready_callback)));

      installer->Register(cus, std::move(on_registered_callback));
    }
  }
}

void RegisterSodaLanguageComponent(ComponentUpdateService* cus,
                                   PrefService* profile_prefs,
                                   PrefService* global_prefs,
                                   base::OnceClosure on_ready_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption) &&
      base::FeatureList::IsEnabled(media::kLiveCaption)) {
    if (profile_prefs->GetBoolean(prefs::kLiveCaptionEnabled)) {
      base::Optional<speech::SodaLanguagePackComponentConfig> config =
          speech::GetLanguageComponentConfig(
              profile_prefs->GetString(prefs::kLiveCaptionLanguageCode));
      if (config) {
        RegisterSodaLanguagePackComponent(config.value(), cus, global_prefs,
                                          std::move(on_ready_callback));
      }
    }
  }
}

}  // namespace component_updater
