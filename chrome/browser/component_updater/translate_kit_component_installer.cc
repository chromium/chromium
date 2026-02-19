// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/translate_kit_component_installer.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/on_device_translation/constants.h"
#include "components/on_device_translation/features.h"
#include "components/on_device_translation/public/paths.h"
#include "components/on_device_translation/public/pref_names.h"
#include "components/update_client/update_client_errors.h"
#include "crypto/sha2.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/dbus/image_loader/image_loader_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace component_updater {
namespace {

// The location of the libtranslatekit binary within the installation directory.
#if BUILDFLAG(IS_WIN)
constexpr base::FilePath::CharType kTranslateKitBinaryRelativePath[] =
    FILE_PATH_LITERAL("TranslateKitFiles/libtranslatekit.dll");
#else
constexpr base::FilePath::CharType kTranslateKitBinaryRelativePath[] =
    FILE_PATH_LITERAL("TranslateKitFiles/libtranslatekit.so");
#endif

// The manifest name of the TranslateKit component.
// This matches:
// - the manifest name in Automation.java from
//   go/newchromecomponent#server-side-setup.
// - the display name at http://omaharelease/2134318/settings.
constexpr char kTranslateKitManifestName[] = "Chrome TranslateKit";

// Returns the full path where the libtranslatekit binary will be installed.
//
// The installation path is under
//    <UserDataDir>/TranslateKit/lib/<version>/TranslateKitFiles/libtranslatekit.xx
// where <User Data Dir> can be determined by following the guide:
// https://chromium.googlesource.com/chromium/src.git/+/HEAD/docs/user_data_dir.md#current-location
base::FilePath GetInstalledPath(const base::FilePath& base) {
  return base.Append(kTranslateKitBinaryRelativePath);
}

#if BUILDFLAG(IS_CHROMEOS)
constexpr base::FilePath::CharType kCrosSquashfsFilename[] =
    FILE_PATH_LITERAL("image.squash");

constexpr base::FilePath::CharType kTranslateKitImageLoaderName[] =
    FILE_PATH_LITERAL("ChromeTranslateKit");

base::FilePath GetSquashFsImagePath(const base::FilePath& base) {
  return base.Append(kCrosSquashfsFilename);
}
#endif

void SetBinaryPathInPrefs(PrefService* pref_service,
                          const base::FilePath& install_dir) {
  auto installed_path = GetInstalledPath(install_dir);
  pref_service->SetFilePath(prefs::kTranslateKitBinaryPath, installed_path);
}

}  // namespace

TranslateKitComponentInstallerPolicy::TranslateKitComponentInstallerPolicy(
    PrefService* pref_service,
    base::RepeatingClosure on_ready_callback)
    : pref_service_(pref_service),
      on_ready_callback_(on_ready_callback ? std::move(on_ready_callback)
                                           : base::DoNothing()) {}

TranslateKitComponentInstallerPolicy::~TranslateKitComponentInstallerPolicy() =
    default;

bool TranslateKitComponentInstallerPolicy::VerifyInstallation(
    const base::DictValue& manifest,
    const base::FilePath& install_dir) const {
#if BUILDFLAG(IS_CHROMEOS)
  return base::PathExists(GetSquashFsImagePath(install_dir));
#else
  return base::PathExists(GetInstalledPath(install_dir));
#endif  // BUILDFLAG(IS_CHROMEOS)
}

bool TranslateKitComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool TranslateKitComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
TranslateKitComponentInstallerPolicy::OnCustomInstall(
    const base::DictValue& manifest,
    const base::FilePath& install_dir) {
  // Nothing custom here.
  return update_client::CrxInstaller::Result(0);
}

void TranslateKitComponentInstallerPolicy::OnCustomUninstall() {}

void TranslateKitComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::DictValue manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();

  CHECK(pref_service_);

#if BUILDFLAG(IS_CHROMEOS)
  if (ash::ImageLoaderClient::Get()) {
    ash::ImageLoaderClient::Get()->LoadComponentAtPath(
        kTranslateKitImageLoaderName, install_dir,
        base::BindOnce(
            &TranslateKitComponentInstallerPolicy::OnImageLoaderComponentLoaded,
            weak_factory_.GetWeakPtr()));
  } else {
    LOG(ERROR) << "ash::ImageLoaderClient not available.";
  }
#else
  SetBinaryPathInPrefs(pref_service_, install_dir);
  on_ready_callback_.Run();
#endif  // BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(IS_CHROMEOS)
void TranslateKitComponentInstallerPolicy::OnImageLoaderComponentLoaded(
    std::optional<base::FilePath> mount_path) {
  if (!mount_path.has_value() || mount_path->empty()) {
    base::UmaHistogramEnumeration("ComponentUpdater.TranslateKit.MountError",
                                  update_client::Error::INVALID_ARGUMENT);
    return;
  }
  SetBinaryPathInPrefs(pref_service_, *mount_path);
  on_ready_callback_.Run();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

base::FilePath TranslateKitComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return on_device_translation::GetBinaryRelativeInstallDir();
}

void TranslateKitComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kTranslateKitPublicKeySHA256),
               std::end(kTranslateKitPublicKeySHA256));
}

std::string TranslateKitComponentInstallerPolicy::GetName() const {
  return kTranslateKitManifestName;
}

update_client::InstallerAttributes
TranslateKitComponentInstallerPolicy::GetInstallerAttributes() const {
  update_client::InstallerAttributes installer_attributes;
#if defined(MEMORY_SANITIZER)
  installer_attributes["is_msan"] = "true";
#endif  // defined(MEMORY_SANITIZER)
  return installer_attributes;
}

// static
void TranslateKitComponentInstallerPolicy::UpdateComponentOnDemand(
    ComponentUpdateService* cus) {
  auto crx_id = GetExtensionId();
  cus->GetOnDemandUpdater().OnDemandUpdate(
      crx_id, component_updater::OnDemandUpdater::Priority::FOREGROUND,
      base::BindOnce([](update_client::Error error) {
        if (error != update_client::Error::NONE &&
            error != update_client::Error::UPDATE_IN_PROGRESS) {
          base::UmaHistogramEnumeration(
              "ComponentUpdater.TranslateKit.UpdateError", error);
        }
      }));
}
// static
const std::string TranslateKitComponentInstallerPolicy::GetExtensionId() {
  return crx_file::id_util::GenerateIdFromHash(kTranslateKitPublicKeySHA256);
}

void RegisterTranslateKitComponent(ComponentUpdateService* cus,
                                   PrefService* pref_service,
                                   bool force_install,
                                   base::OnceClosure registered_callback,
                                   base::RepeatingClosure on_ready_callback) {
  VLOG(1) << "Registering TranslateKit component.";
  if (!force_install &&
      !pref_service->GetBoolean(prefs::kTranslateKitPreviouslyRegistered)) {
    return;
  }

  // If the component is already installed, do nothing.
  const std::vector<std::string> component_ids = cus->GetComponentIDs();
  if (std::find(component_ids.begin(), component_ids.end(),
                TranslateKitComponentInstallerPolicy::GetExtensionId()) !=
      component_ids.end()) {
    return;
  }

  pref_service->SetBoolean(prefs::kTranslateKitPreviouslyRegistered, true);
  // Clear any previously set path in the preferences
  SetBinaryPathInPrefs(pref_service, base::FilePath());

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<TranslateKitComponentInstallerPolicy>(
          pref_service, std::move(on_ready_callback)));
  installer->Register(cus, std::move(registered_callback));
}

}  // namespace component_updater
