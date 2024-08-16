// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/component_updater/soda_component_installer.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/soda_language_pack_component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/soda/constants.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <aclapi.h>

#include "base/win/scoped_localalloc.h"
#include "base/win/sid.h"
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

static_assert(std::size(kSodaPublicKeySHA256) == crypto::kSHA256Length,
              "Wrong hash length");

constexpr char kSodaManifestName[] = "SODA Library";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)

constexpr base::FilePath::CharType kSodaIndicatorFile[] =
#if defined(ARCH_CPU_X86)
    FILE_PATH_LITERAL("SODAFiles/arch_x86");
#elif defined(ARCH_CPU_X86_64)
    FILE_PATH_LITERAL("SODAFiles/arch_x64");
#elif defined(ARCH_CPU_ARM64)
    FILE_PATH_LITERAL("SODAFiles/arch_arm64");
#else
    {};
#endif

static_assert(sizeof(kSodaIndicatorFile) > 0, "Unknown CPU architecture.");

#endif

}  // namespace

SodaComponentInstallerPolicy::SodaComponentInstallerPolicy(
    OnSodaComponentInstalledCallback on_installed_callback,
    OnSodaComponentReadyCallback on_ready_callback)
    : on_installed_callback_(on_installed_callback),
      on_ready_callback_(std::move(on_ready_callback)) {}

SodaComponentInstallerPolicy::~SodaComponentInstallerPolicy() = default;

const std::string SodaComponentInstallerPolicy::GetExtensionId() {
  return crx_file::id_util::GenerateIdFromHash(kSodaPublicKeySHA256);
}

void SodaComponentInstallerPolicy::UpdateSodaComponentOnDemand() {
  const std::string crx_id =
      component_updater::SodaComponentInstallerPolicy::GetExtensionId();
  g_browser_process->component_updater()->GetOnDemandUpdater().OnDemandUpdate(
      crx_id, component_updater::OnDemandUpdater::Priority::FOREGROUND,
      base::BindOnce([](update_client::Error error) {
        if (error != update_client::Error::NONE &&
            error != update_client::Error::UPDATE_IN_PROGRESS) {
          LOG(ERROR) << "On demand update of the SODA component failed "
                        "with error: "
                     << static_cast<int>(error);
        }
      }));
}

update_client::CrxInstaller::Result
SodaComponentInstallerPolicy::SetComponentDirectoryPermission(
    const base::FilePath& install_dir) {
#if BUILDFLAG(IS_WIN)
  const std::optional<base::win::Sid> users_sid =
      base::win::Sid::FromKnownSid(base::win::WellKnownSid::kBuiltinUsers);
  if (!users_sid) {
    return update_client::CrxInstaller::Result(
        update_client::InstallError::SET_PERMISSIONS_FAILED);
  }

  // Initialize an EXPLICIT_ACCESS structure for an ACE.
  EXPLICIT_ACCESS explicit_access[1] = {};
  explicit_access[0].grfAccessPermissions = GENERIC_READ | GENERIC_EXECUTE;
  explicit_access[0].grfAccessMode = GRANT_ACCESS;
  explicit_access[0].grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
  explicit_access[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
  explicit_access[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
  explicit_access[0].Trustee.ptstrName =
      reinterpret_cast<LPTSTR>(users_sid->GetPSID());

  PACL acl_ptr = nullptr;
  if (::SetEntriesInAcl(std::size(explicit_access), explicit_access, nullptr,
                        &acl_ptr) != ERROR_SUCCESS) {
    return update_client::CrxInstaller::Result(
        update_client::InstallError::SET_PERMISSIONS_FAILED);
  }
  base::win::ScopedLocalAllocTyped<ACL> acl =
      base::win::TakeLocalAlloc(acl_ptr);

  // Change the security attributes.
  LPWSTR file_name = const_cast<LPWSTR>(install_dir.value().c_str());
  if (::SetNamedSecurityInfo(file_name, SE_FILE_OBJECT,
                             DACL_SECURITY_INFORMATION, nullptr, nullptr,
                             acl.get(), nullptr) != ERROR_SUCCESS) {
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
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return SodaComponentInstallerPolicy::SetComponentDirectoryPermission(
      install_dir);
}

void SodaComponentInstallerPolicy::OnCustomUninstall() {}

bool SodaComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
  bool missing_indicator_file =
      !base::PathExists(install_dir.Append(kSodaIndicatorFile));

  base::UmaHistogramBoolean(
      "Accessibility.LiveCaption.SodaVerificationFailureMissingIndicatorFile",
      missing_indicator_file);

  if (missing_indicator_file) {
    return false;
  }
#endif

  return base::PathExists(install_dir.Append(speech::kSodaBinaryRelativePath));
}

void SodaComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();
  if (on_installed_callback_) {
    on_installed_callback_.Run(install_dir);
  }

  if (on_ready_callback_) {
    std::move(on_ready_callback_).Run();
  }
}

base::FilePath SodaComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(speech::kSodaInstallationRelativePath);
}

void SodaComponentInstallerPolicy::GetHash(std::vector<uint8_t>* hash) const {
  hash->assign(kSodaPublicKeySHA256,
               kSodaPublicKeySHA256 + std::size(kSodaPublicKeySHA256));
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
#if !BUILDFLAG(IS_ANDROID)
  prefs->SetFilePath(prefs::kSodaBinaryPath,
                     install_dir.Append(speech::kSodaBinaryRelativePath));
#endif
}

void RegisterSodaComponent(ComponentUpdateService* cus,
                           PrefService* global_prefs,
                           base::OnceClosure on_ready_callback,
                           base::OnceClosure on_registered_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (captions::IsLiveCaptionFeatureSupported()) {
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

void RegisterSodaLanguageComponent(
    ComponentUpdateService* cus,
    const std::string& language,
    PrefService* global_prefs,
    OnSodaLanguagePackComponentReadyCallback on_ready_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (captions::IsLiveCaptionFeatureSupported()) {
    std::optional<speech::SodaLanguagePackComponentConfig> config =
        speech::GetLanguageComponentConfig(language);
    if (config) {
      RegisterSodaLanguagePackComponent(config.value(), cus, global_prefs,
                                        std::move(on_ready_callback));
    }
  }
}

}  // namespace component_updater
