// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/widevine_cdm_component_installer.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/media/cdm_manifest.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cdm_registry.h"
#include "content/public/common/cdm_info.h"
#include "content/public/common/content_paths.h"
#include "crypto/sha2.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "chrome/common/media/component_widevine_cdm_hint_file_linux.h"
#endif

#if defined(OS_MAC)
#include "base/mac/mach_o.h"
#include "base/mac/rosetta.h"
#endif  // OS_MAC

#if !BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
#error This file should only be compiled when Widevine CDM component is enabled
#endif

using content::BrowserThread;
using content::CdmRegistry;

namespace component_updater {

namespace {

static bool g_was_widevine_cdm_component_rejected_due_to_no_rosetta;

// CRX hash. The extension id is: oimompecagnajdejgnnjijobebaeigek.
const uint8_t kWidevineSha2Hash[] = {
    0xe8, 0xce, 0xcf, 0x42, 0x06, 0xd0, 0x93, 0x49, 0x6d, 0xd9, 0x89,
    0xe1, 0x41, 0x04, 0x86, 0x4a, 0x8f, 0xbd, 0x86, 0x12, 0xb9, 0x58,
    0x9b, 0xfb, 0x4f, 0xbb, 0x1b, 0xa9, 0xd3, 0x85, 0x37, 0xef};
static_assert(base::size(kWidevineSha2Hash) == crypto::kSHA256Length,
              "Wrong hash length");

// Name of the Widevine CDM OS in the component manifest.
const char kWidevineCdmPlatform[] =
#if defined(OS_MAC)
    "mac";
#elif defined(OS_WIN)
    "win";
#elif defined(OS_CHROMEOS)
    "cros";
#elif defined(OS_LINUX)
    "linux";
#else
#error This file should only be included for supported platforms.
#endif

// Name of the Widevine CDM architecture in the component manifest.
const char kWidevineCdmArch[] =
#if defined(ARCH_CPU_X86)
    "x86";
#elif defined(ARCH_CPU_X86_64)
    "x64";
#elif defined(ARCH_CPU_ARMEL)
    "arm";
#elif defined(ARCH_CPU_ARM64)
    "arm64";
#else
#error This file should only be included for supported architecture.
#endif

// Widevine CDM is packaged as a multi-CRX. Widevine CDM binaries are located in
// _platform_specific/<platform_arch> folder in the package. This function
// returns the platform-specific subdirectory that is part of that multi-CRX.
base::FilePath GetPlatformDirectory(const base::FilePath& base_path) {
  std::string platform_arch = kWidevineCdmPlatform;
  platform_arch += '_';
  platform_arch += kWidevineCdmArch;
  return base_path.AppendASCII("_platform_specific").AppendASCII(platform_arch);
}

#if !defined(OS_LINUX) && !defined(OS_CHROMEOS)
// On Linux the Widevine CDM is loaded at startup before the zygote is locked
// down. As a result there is no need to register the CDM with Chrome as it
// can't be used until Chrome is restarted. Instead we simply update the hint
// file so that startup can load this new version next time.
void RegisterWidevineCdmWithChrome(
    const base::Version& cdm_version,
    const base::FilePath& cdm_path,
#if defined(OS_MAC) && defined(ARCH_CPU_ARM64)
    bool launch_x86_64,
#endif
    std::unique_ptr<base::DictionaryValue> manifest) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // This check must be a subset of the check in VerifyInstallation() to
  // avoid the case where the CDM is accepted by the component updater
  // but not registered.
  content::CdmCapability capability;
  if (!ParseCdmManifest(*manifest, &capability)) {
    VLOG(1) << "Not registering Widevine CDM due to malformed manifest.";
    return;
  }

  VLOG(1) << "Register Widevine CDM with Chrome";
  content::CdmInfo cdm_info(kWidevineCdmDisplayName, kWidevineCdmGuid,
                            cdm_version, cdm_path, kWidevineCdmFileSystemId,
                            std::move(capability), kWidevineKeySystem, false);
#if defined(OS_MAC) && defined(ARCH_CPU_ARM64)
  cdm_info.launch_x86_64 = launch_x86_64;
#endif  // OS_MAC && ARCH_CPU_ARM64
  CdmRegistry::GetInstance()->RegisterCdm(cdm_info);
}
#endif  // !defined(OS_LINUX) && !defined(OS_CHROMEOS)

base::FilePath GetCdmPathFromInstallDir(const base::FilePath& install_dir) {
  base::FilePath cdm_platform_dir = GetPlatformDirectory(install_dir);
  std::string cdm_lib_name =
      base::GetNativeLibraryName(kWidevineCdmLibraryName);
  base::FilePath cdm_path = cdm_platform_dir.AppendASCII(cdm_lib_name);

#if defined(OS_MAC) && defined(ARCH_CPU_ARM64)
  // If no Widevine CDM is present in the normal native arm64 location, look for
  // one in the x86_64 location. Beware that the architecture embedded in the
  // pathname does not necessarily indicate what architecture the Mach-O file at
  // that path supports: an x86_64 library may be found in an arm64 location. A
  // separate base::GetMachOArchitectures call must be made to determine the
  // actual architecture.
  //
  // If there is no file at all in the native arm64 location, fall back to the
  // x86_64 location. VerifyInstallation() and UpdateCdmPath() will do Rosetta
  // checks before actually using it.
  if (!base::PathExists(cdm_path) &&
      base::EndsWith(cdm_platform_dir.value(), kWidevineCdmArch)) {
    cdm_platform_dir = base::FilePath(
        cdm_platform_dir.value().substr(
            0, cdm_platform_dir.value().size() - strlen(kWidevineCdmArch)) +
        "x64");
    cdm_path = cdm_platform_dir.AppendASCII(cdm_lib_name);
  }
#endif  // OS_MAC && ARCH_CPU_ARM64

  return cdm_path;
}

}  // namespace

class WidevineCdmComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  WidevineCdmComponentInstallerPolicy();
  ~WidevineCdmComponentInstallerPolicy() override = default;

 private:
  // The following methods override ComponentInstallerPolicy.
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& path,
                      std::unique_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
  std::vector<std::string> GetMimeTypes() const override;

  // Updates CDM path if necessary.
  void UpdateCdmPath(const base::Version& cdm_version,
                     const base::FilePath& cdm_install_dir,
                     std::unique_ptr<base::DictionaryValue> manifest);

  DISALLOW_COPY_AND_ASSIGN(WidevineCdmComponentInstallerPolicy);
};

WidevineCdmComponentInstallerPolicy::WidevineCdmComponentInstallerPolicy() =
    default;

bool WidevineCdmComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool WidevineCdmComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
WidevineCdmComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);
}

void WidevineCdmComponentInstallerPolicy::OnCustomUninstall() {}

// Once the CDM is ready, update the CDM path.
void WidevineCdmComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& path,
    std::unique_ptr<base::DictionaryValue> manifest) {
  if (!IsCdmManifestCompatibleWithChrome(*manifest)) {
    VLOG(1) << "Installed Widevine CDM component is incompatible.";
    return;
  }

  // Widevine CDM affects encrypted media playback, hence USER_VISIBLE.
  // See http://crbug.com/900169 for the context.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&WidevineCdmComponentInstallerPolicy::UpdateCdmPath,
                     base::Unretained(this), version, path,
                     base::Passed(&manifest)));
}

bool WidevineCdmComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  base::FilePath cdm_path = GetCdmPathFromInstallDir(install_dir);

#if defined(OS_MAC) && defined(ARCH_CPU_ARM64)
  // Before committing to run an x86_64 version of Widevine under Rosetta, make
  // sure that Rosetta is actually available. It’s not installed by default.
  const base::MachOArchitectures architectures =
      base::GetMachOArchitectures(cdm_path);
  const bool launch_x86_64 =
      (architectures & (base::MachOArchitectures::kX86_64 |
                        base::MachOArchitectures::kARM64)) ==
      base::MachOArchitectures::kX86_64;
  if (launch_x86_64 && !base::mac::IsRosettaInstalled()) {
    g_was_widevine_cdm_component_rejected_due_to_no_rosetta = true;
    return false;
  }
  g_was_widevine_cdm_component_rejected_due_to_no_rosetta = false;
#endif  // OS_MAC && ARCH_CPU_ARM64

  content::CdmCapability capability;
  return IsCdmManifestCompatibleWithChrome(manifest) &&
         base::PathExists(cdm_path) && ParseCdmManifest(manifest, &capability);
}

// The base directory on Windows looks like:
// <profile>\AppData\Local\Google\Chrome\User Data\WidevineCdm\.
base::FilePath WidevineCdmComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath::FromUTF8Unsafe(kWidevineCdmBaseDirectory);
}

void WidevineCdmComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(kWidevineSha2Hash,
               kWidevineSha2Hash + base::size(kWidevineSha2Hash));
}

std::string WidevineCdmComponentInstallerPolicy::GetName() const {
  return kWidevineCdmDisplayName;
}

update_client::InstallerAttributes
WidevineCdmComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

std::vector<std::string> WidevineCdmComponentInstallerPolicy::GetMimeTypes()
    const {
  return std::vector<std::string>();
}

void WidevineCdmComponentInstallerPolicy::UpdateCdmPath(
    const base::Version& cdm_version,
    const base::FilePath& cdm_install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  // On some platforms (e.g. Mac) we use symlinks for paths. Convert paths to
  // absolute paths to avoid unexpected failure. base::MakeAbsoluteFilePath()
  // requires IO so it can only be done in this function.
  const base::FilePath absolute_cdm_install_dir =
      base::MakeAbsoluteFilePath(cdm_install_dir);
  if (absolute_cdm_install_dir.empty()) {
    PLOG(WARNING) << "Failed to get absolute CDM install path.";
    return;
  }

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  VLOG(1) << "Updating hint file with Widevine CDM " << cdm_version;

  // This is running on a thread that allows IO, so simply update the hint file.
  if (!UpdateWidevineCdmHintFile(cdm_install_dir))
    PLOG(WARNING) << "Failed to update Widevine CDM hint path.";
#else
  base::FilePath cdm_path = GetCdmPathFromInstallDir(absolute_cdm_install_dir);

#if defined(OS_MAC) && defined(ARCH_CPU_ARM64)
  // Detect the architecture of the chosen Widevine CDM. If it contains x86_64
  // code and no arm64 code, arrange to run it via Rosetta translation.
  // Detection is necessary because the library may contain only x86_64 code
  // even if loaded from an arm64 path. This isn’t likely for the bundled
  // Widevine CDM, but can happen with a component-updated version.
  const base::MachOArchitectures architectures =
      base::GetMachOArchitectures(cdm_path);
  const bool launch_x86_64 =
      (architectures & (base::MachOArchitectures::kX86_64 |
                        base::MachOArchitectures::kARM64)) ==
      base::MachOArchitectures::kX86_64;

  // In order for this strategy to work, Rosetta must be installed. That should
  // be guaranteed by VerifyInstallation succeeding.
  if (launch_x86_64) {
    DCHECK(base::mac::IsRosettaInstalled());

    // To avoid a long delay (15 seconds observed is typical) when first loading
    // the Widevine CDM under Rosetta, submit required modules for ahead-of-time
    // translation. The necessary modules are:
    //  - the helper executable to launch,
    //  - the framework that contains the vast majority of the code, and
    //  - the Widevine CDM library itself.
    // If Rosetta’s translation cache for these modules is already current, they
    // will not be re-translated. If anything requires translation, it will
    // still be time-consuming, but it’ll happen on a background thread without
    // bothering the user, hopefully before the user needs to use them. If these
    // modules are needed before the translation is complete, translation will
    // at least have had a head start.

    std::vector<base::FilePath> rosetta_translate_paths;
    base::FilePath helper_path;
    if (base::PathService::Get(content::CHILD_PROCESS_EXE, &helper_path)) {
      rosetta_translate_paths.push_back(helper_path);
    }

    base::FilePath framework_path;
    if (base::PathService::Get(base::FILE_MODULE, &framework_path)) {
      rosetta_translate_paths.push_back(framework_path);
    }

    rosetta_translate_paths.push_back(cdm_path);

    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(base::IgnoreResult(
                           &base::mac::RequestRosettaAheadOfTimeTranslation),
                       std::move(rosetta_translate_paths)));
  }
#endif  // OS_MAC && ARCH_CPU_ARM64

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&RegisterWidevineCdmWithChrome, cdm_version, cdm_path,
#if defined(OS_MAC) && defined(ARCH_CPU_ARM64)
                     launch_x86_64,
#endif  // OS_MAC && ARCH_CPU_ARM64
                     base::Passed(&manifest)));
#endif
}

void RegisterWidevineCdmComponent(ComponentUpdateService* cus) {
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<WidevineCdmComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

bool WasWidevineCdmComponentRejectedDueToNoRosetta() {
  return g_was_widevine_cdm_component_rejected_due_to_no_rosetta;
}

}  // namespace component_updater
