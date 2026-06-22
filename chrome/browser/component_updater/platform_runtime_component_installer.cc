// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/platform_runtime_component_installer.h"

#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/platform_runtime/platform_runtime_impl.h"
#include "components/component_updater/component_updater_paths.h"
#include "crypto/sha2.h"

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The component id is: jidecimafobogahglicpmeajcaaaibib
const uint8_t kPlatformRuntimePublicKeySHA256[32] = {
    0x98, 0x34, 0x28, 0xc0, 0x5e, 0x1e, 0x60, 0x76, 0xb8, 0x2f, 0xc4,
    0x09, 0x20, 0x00, 0x81, 0x81, 0x76, 0x2f, 0x59, 0xa6, 0x57, 0x67,
    0x42, 0xd1, 0xfe, 0xdf, 0xd0, 0x28, 0x86, 0x10, 0xef, 0xf0};

static_assert(std::size(kPlatformRuntimePublicKeySHA256) ==
              crypto::kSHA256Length);
const char kPlatformRuntimeManifestName[] = "Platform Runtime";

base::FilePath GetBinaryPath(const base::FilePath& install_dir) {
  return install_dir.AppendASCII(
      base::GetNativeLibraryName("chrome_platform_runtime"));
}

}  // namespace

namespace component_updater {

BASE_FEATURE(kEnablePlatformRuntimeComponent,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool PlatformRuntimeComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

bool PlatformRuntimeComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
PlatformRuntimeComponentInstallerPolicy::OnCustomInstall(
    const base::DictValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void PlatformRuntimeComponentInstallerPolicy::OnCustomUninstall() {}

bool PlatformRuntimeComponentInstallerPolicy::VerifyInstallation(
    const base::DictValue& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(GetBinaryPath(install_dir));
}

void PlatformRuntimeComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::DictValue manifest) {
  VLOG(1) << "Platform Runtime component ready, version " << version.GetString()
          << " in " << install_dir.value();

  // Load the library in the Browser Process (where Component Updater runs).
  // Post it to a background thread because loading a DLL involves blocking I/O.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          [](const base::FilePath& path) {
            platform_runtime::PlatformRuntimeImpl::GetInstance()
                ->UpdatePlatformRuntimeLibrary(path);
          },
          GetBinaryPath(install_dir)));
}

base::FilePath PlatformRuntimeComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("PlatformRuntime"));
}

void PlatformRuntimeComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kPlatformRuntimePublicKeySHA256),
               std::end(kPlatformRuntimePublicKeySHA256));
}

std::string PlatformRuntimeComponentInstallerPolicy::GetName() const {
  return kPlatformRuntimeManifestName;
}

update_client::InstallerAttributes
PlatformRuntimeComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void MaybeRegisterPlatformRuntimeComponent(ComponentUpdateService* cus) {
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (base::FeatureList::IsEnabled(kEnablePlatformRuntimeComponent)) {
    VLOG(1) << "Registering Platform Runtime component.";
    auto installer = base::MakeRefCounted<ComponentInstaller>(
        std::make_unique<PlatformRuntimeComponentInstallerPolicy>());
    installer->Register(cus, base::OnceClosure());
  }
#endif
}

}  // namespace component_updater
