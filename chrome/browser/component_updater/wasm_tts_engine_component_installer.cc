// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/wasm_tts_engine_component_installer.h"

#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/accessibility/embedded_a11y_extension_loader.h"
#include "chrome/common/extensions/extension_constants.h"
#include "ui/accessibility/accessibility_features.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

using component_updater::ComponentUpdateService;

namespace {

const base::FilePath::CharType kManifestFileName[] =
    FILE_PATH_LITERAL("wasm_tts_manifest.json");
const base::FilePath::CharType kBindingsMainWasmFileName[] =
    FILE_PATH_LITERAL("bindings_main.wasm");
const base::FilePath::CharType kBindingsMainJsFileName[] =
    FILE_PATH_LITERAL("bindings_main.js");
const base::FilePath::CharType kTTSEngineJsBinFileName[] =
    FILE_PATH_LITERAL("googletts_engine_js_bin.js");
const base::FilePath::CharType kWorkletProcessorJsFileName[] =
    FILE_PATH_LITERAL("streaming_worklet_processor.js");
const base::FilePath::CharType kVoicesJsonFileName[] =
    FILE_PATH_LITERAL("voices.json");

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: bjbcblmdcnggnibecjikpoljcgkbgphl
constexpr std::array<uint8_t, 32> kWasmTtsEnginePublicKeySHA256 = {
    0x19, 0x12, 0x1b, 0xc3, 0x2d, 0x66, 0xd8, 0x14, 0x29, 0x8a, 0xfe,
    0xb9, 0x26, 0xa1, 0x6f, 0x7b, 0xc2, 0x14, 0x17, 0xf1, 0xb0, 0x1e,
    0x56, 0x89, 0xcb, 0x53, 0x8e, 0x13, 0x92, 0xc1, 0x44, 0x5d};

const char kWasmTtsEngineManifestName[] = "WASM TTS Engine";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
class WasmTTSEngineDirectory {
 public:
  static WasmTTSEngineDirectory* Get() {
    static base::NoDestructor<WasmTTSEngineDirectory> wasm_directory;
    return wasm_directory.get();
  }

  void Get(base::OnceCallback<void(const base::FilePath&)> callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    callbacks_.push_back(std::move(callback));
    if (!dir_.empty()) {
      FireCallbacks();
    }
  }

  void Set(const base::FilePath& new_dir) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    CHECK(!new_dir.empty());
    dir_ = new_dir;
    FireCallbacks();
  }

 private:
  void FireCallbacks() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    CHECK(!dir_.empty());
    std::vector<base::OnceCallback<void(const base::FilePath&)>> callbacks;
    std::swap(callbacks, callbacks_);
    for (base::OnceCallback<void(const base::FilePath&)>& callback :
         callbacks) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), dir_));
    }
  }

  base::FilePath dir_;
  std::vector<base::OnceCallback<void(const base::FilePath&)>> callbacks_;
};
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

}  // namespace

namespace component_updater {

bool WasmTtsEngineComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool WasmTtsEngineComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
WasmTtsEngineComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& /* manifest */,
    const base::FilePath& /* install_dir */) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void WasmTtsEngineComponentInstallerPolicy::OnCustomUninstall() {}

void WasmTtsEngineComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict /* manifest */) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (features::IsWasmTtsComponentUpdaterEnabled() &&
      !features::IsWasmTtsEngineAutoInstallDisabled()) {
    // Instead of installing the component extension as soon as it is ready,
    // store the install directory, so that the install can be triggered
    // via ReadAnythingService once the side panel has been opened. This
    // prevents the extension from being installed unnecessarily for those
    // who aren't using reading mode.
    WasmTTSEngineDirectory* wasm_directory = WasmTTSEngineDirectory::Get();
    wasm_directory->Set(install_dir);
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
}

// Called during startup and installation before ComponentReady().
bool WasmTtsEngineComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& /* manifest */,
    const base::FilePath& install_dir) const {
  return base::PathExists(install_dir.Append(kManifestFileName)) &&
         base::PathExists(install_dir.Append(kBindingsMainWasmFileName)) &&
         base::PathExists(install_dir.Append(kBindingsMainJsFileName)) &&
         base::PathExists(install_dir.Append(kTTSEngineJsBinFileName)) &&
         base::PathExists(install_dir.Append(kWorkletProcessorJsFileName)) &&
         base::PathExists(install_dir.Append(kVoicesJsonFileName));
}

base::FilePath WasmTtsEngineComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("WasmTtsEngine"));
}

void WasmTtsEngineComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kWasmTtsEnginePublicKeySHA256),
               std::end(kWasmTtsEnginePublicKeySHA256));
}

std::string WasmTtsEngineComponentInstallerPolicy::GetName() const {
  return kWasmTtsEngineManifestName;
}

update_client::InstallerAttributes
WasmTtsEngineComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterWasmTtsEngineComponent(ComponentUpdateService* cus) {
  VLOG(1) << "Registering WASM TTS Engine component.";
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<WasmTtsEngineComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

void WasmTtsEngineComponentInstallerPolicy::GetWasmTTSEngineDirectory(
    base::OnceCallback<void(const base::FilePath&)> callback) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  WasmTTSEngineDirectory* wasm_directory = WasmTTSEngineDirectory::Get();
  wasm_directory->Get(std::move(callback));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
}

}  // namespace component_updater
