// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/wasm_tts_engine_component_installer.h"

#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "base/no_destructor.h"
#include "chrome/browser/accessibility/embedded_a11y_extension_loader.h"
#include "chrome/common/extensions/extension_constants.h"
#include "ui/accessibility/accessibility_features.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

namespace {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
const base::FilePath::CharType kBindingsMainWasmFileName[] =
    FILE_PATH_LITERAL("bindings_main.wasm");
const base::FilePath::CharType kBindingsMainJsFileName[] =
    FILE_PATH_LITERAL("bindings_main.js");
const base::FilePath::CharType kWorkletProcessorJsFileName[] =
    FILE_PATH_LITERAL("streaming_worklet_processor.js");
const base::FilePath::CharType kVoicesJsonFileName[] =
    FILE_PATH_LITERAL("voices.json");
const base::FilePath::CharType kManifestV3FileName[] =
    FILE_PATH_LITERAL("wasm_tts_manifest_v3.json");
const base::FilePath::CharType kOffscreenHtmlFileName[] =
    FILE_PATH_LITERAL("offscreen.html");
const base::FilePath::CharType kOffscreenCompiledFileName[] =
    FILE_PATH_LITERAL("offscreen_compiled.js");
const base::FilePath::CharType kBackgroundCompiledFileName[] =
    FILE_PATH_LITERAL("background_compiled.js");
#endif

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

  bool IsSet() const {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    return !dir_.empty();
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

WasmTtsEngineComponentInstallerPolicy::WasmTtsEngineComponentInstallerPolicy(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

// static
void WasmTtsEngineComponentInstallerPolicy::RegisterPrefs(
    PrefRegistrySimple* registry) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  registry->RegisterTimePref(prefs::kAccessibilityReadAnythingDateLastOpened,
                             base::Time());
  registry->RegisterBooleanPref(
      prefs::kAccessibilityReadAnythingTTSEngineReinstalled, false);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
}

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
  if (!features::IsWasmTtsEngineAutoInstallDisabled()) {
    // Instead of installing the component extension as soon as it is ready,
    // store the install directory, so that the install can be triggered
    // via ReadAnythingService once the side panel has been opened. This
    // prevents the extension from being installed unnecessarily for those
    // who aren't using reading mode.
    // An exception is made for reinstalls at THRESHOLD_RECENT and
    // THRESHOLD_LONGER amounts of time since reading mode was last opened. At
    // these points, the WASM TTS Engine is reinstalled in order to clean up
    // any removed voices.
    WasmTTSEngineDirectory* wasm_directory = WasmTTSEngineDirectory::Get();
    wasm_directory->Set(install_dir);

    MaybeReinstallTtsEngine(install_dir);
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
}

// In order to uninstall unused voices when reading mode has been unused for an
// extended period of time, Chrome needs to reinstall the TTS engine. This will
// be removed the next time Chrome is restarted.
void WasmTtsEngineComponentInstallerPolicy::MaybeReinstallTtsEngine(
    const base::FilePath& install_dir) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  const base::Time current_time = base::Time::Now();
  const base::Time date_last_opened =
      pref_service_->GetTime(prefs::kAccessibilityReadAnythingDateLastOpened);
  const bool previously_reinstalled = pref_service_->GetBoolean(
      prefs::kAccessibilityReadAnythingTTSEngineReinstalled);

  // Reading mode hasn't been opened in the last 90 days, so don't reinstall
  // the engine.
  if (date_last_opened.is_null()) {
    return;
  }

  // Reading mode was opened more than 14 days ago and less than 90 days ago
  // but the engine was already installed after 14 days to remove unused voices.
  // Reading mode doesn't need to re-install the engine to clean up voices
  // until 90 days have passed.
  if (previously_reinstalled &&
      (current_time - date_last_opened) < kThresholdLonger) {
    return;
  }

  // Reading mode was opened in the last 14 days, so don't reinstall the engine.
  if ((current_time - date_last_opened) < kThresholdRecent) {
    return;
  }

  const base::FilePath::CharType* manifest_file = kManifestV3FileName;

  // If it's been more than 14 days since reading mode was last opened,
  // re-install the engine so that unused voices can be removed.
  EmbeddedA11yExtensionLoader::GetInstance()->Init();
  EmbeddedA11yExtensionLoader::GetInstance()->InstallExtensionWithIdAndPath(
      extension_misc::kComponentUpdaterTTSEngineExtensionId, install_dir,
      manifest_file,
      /*should_localize=*/false);

  // If reading mode hasn't been opened in longer than 14 days but less than
  // 90 days, update that the TTS engine has been reinstalled to prevent
  // subsequent reinstalls from day 14 to day 90.
  if (!previously_reinstalled) {
    pref_service_->SetBoolean(
        prefs::kAccessibilityReadAnythingTTSEngineReinstalled, true);
  }

  // If it's been more than 90 days, clear both preferences for reading mode
  // last opened state. The engine will now not be reinstalled until reading
  // mode is reopened.
  if ((current_time - date_last_opened) >= kThresholdLonger) {
    pref_service_->ClearPref(prefs::kAccessibilityReadAnythingDateLastOpened);
    pref_service_->ClearPref(
        prefs::kAccessibilityReadAnythingTTSEngineReinstalled);
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
}

// Called during startup and installation before ComponentReady().
bool WasmTtsEngineComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& /* manifest */,
    const base::FilePath& install_dir) const {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  return base::PathExists(install_dir.Append(kManifestV3FileName)) &&
         base::PathExists(install_dir.Append(kBindingsMainWasmFileName)) &&
         base::PathExists(install_dir.Append(kBindingsMainJsFileName)) &&
         base::PathExists(install_dir.Append(kOffscreenHtmlFileName)) &&
         base::PathExists(install_dir.Append(kOffscreenCompiledFileName)) &&
         base::PathExists(install_dir.Append(kBackgroundCompiledFileName)) &&
         base::PathExists(install_dir.Append(kWorkletProcessorJsFileName)) &&
         base::PathExists(install_dir.Append(kVoicesJsonFileName));
#else
  // Attempting to install the TTS extension should never be done outside of
  // Windows, Mac, and Linux. Return false as a fallback just in case.
  return false;
#endif
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

void RegisterWasmTtsEngineComponent(ComponentUpdateService* cus,
                                    PrefService* prefs) {
  VLOG(1) << "Registering WASM TTS Engine component.";
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<WasmTtsEngineComponentInstallerPolicy>(prefs));
  installer->Register(cus, base::OnceClosure());
}

void WasmTtsEngineComponentInstallerPolicy::GetWasmTTSEngineDirectory(
    base::OnceCallback<void(const base::FilePath&)> callback) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  WasmTTSEngineDirectory* wasm_directory = WasmTTSEngineDirectory::Get();
  wasm_directory->Get(std::move(callback));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
}

// static
bool WasmTtsEngineComponentInstallerPolicy::IsWasmTTSEngineDirectorySet() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  return WasmTTSEngineDirectory::Get()->IsSet();
#else
  return false;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
}

}  // namespace component_updater
