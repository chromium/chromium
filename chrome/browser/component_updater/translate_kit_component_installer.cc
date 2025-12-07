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
#include "base/logging.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/on_device_translation/constants.h"
#include "chrome/browser/on_device_translation/pref_names.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/services/on_device_translation/public/cpp/features.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/dbus/image_loader/image_loader_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace component_updater {

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The component id is: lbimbicckdokpoicboneldipejkhjgdg
constexpr uint8_t kTranslateKitPublicKeySHA256[32] = {
    0xb1, 0x8c, 0x18, 0x22, 0xa3, 0xea, 0xfe, 0x82, 0x1e, 0xd4, 0xb3,
    0x8f, 0x49, 0xa7, 0x96, 0x36, 0x55, 0xf3, 0xbc, 0x0d, 0xa5, 0x67,
    0x48, 0x09, 0xcd, 0x7b, 0xa9, 0x5f, 0xd8, 0x7f, 0x53, 0xb4};

static_assert(std::size(kTranslateKitPublicKeySHA256) == crypto::kSHA256Length,
              "Wrong hash length");

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
    PrefService* pref_service)
    : pref_service_(pref_service) {}

TranslateKitComponentInstallerPolicy::~TranslateKitComponentInstallerPolicy() =
    default;

bool TranslateKitComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
#if BUILDFLAG(IS_CHROMEOS)
  bool squash_fs_found = base::PathExists(GetSquashFsImagePath(install_dir));
  return squash_fs_found;
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
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  // Nothing custom here.
  return update_client::CrxInstaller::Result(0);
}

void TranslateKitComponentInstallerPolicy::OnCustomUninstall() {}

void TranslateKitComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
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
#endif  // BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(IS_CHROMEOS)
void TranslateKitComponentInstallerPolicy::OnImageLoaderComponentLoaded(
    std::optional<base::FilePath> mount_path) {
  if (!mount_path.has_value() || mount_path->empty()) {
    LOG(ERROR) << "Failed to load TranslateKit component via "
                  "ImageLoaderClient. Mount path invalid.";
    return;
  }
  SetBinaryPathInPrefs(pref_service_, *mount_path);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

base::FilePath TranslateKitComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(
      on_device_translation::kTranslateKitBinaryInstallationRelativeDir);
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
void TranslateKitComponentInstallerPolicy::UpdateComponentOnDemand() {
  auto crx_id = GetExtensionId();
  g_browser_process->component_updater()->GetOnDemandUpdater().OnDemandUpdate(
      crx_id, component_updater::OnDemandUpdater::Priority::FOREGROUND,
      base::BindOnce([](update_client::Error error) {
        if (error != update_client::Error::NONE &&
            error != update_client::Error::UPDATE_IN_PROGRESS) {
          LOG(ERROR) << "Failed to uppdate TranslateKit:"
                     << static_cast<int>(error);
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
                                   base::OnceClosure registered_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
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
      std::make_unique<TranslateKitComponentInstallerPolicy>(pref_service));
  installer->Register(cus, std::move(registered_callback));
}

}  // namespace component_updater
