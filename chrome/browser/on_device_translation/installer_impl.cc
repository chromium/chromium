// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/installer_impl.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/byte_size.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/translate_kit_language_pack_component_installer.h"
#include "chrome/browser/data_saver/data_saver.h"
#include "chrome/browser/on_device_translation/component_update_util.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/on_device_translation/constants.h"
#include "components/on_device_translation/features.h"
#include "components/on_device_translation/installer.h"
#include "components/on_device_translation/metrics.h"
#include "components/on_device_translation/public/language_pack.h"
#include "components/on_device_translation/public/mojom/on_device_translation_service.mojom-forward.h"
#include "components/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/on_device_translation/public/paths.h"
#include "components/on_device_translation/public/pref_names.h"
#include "components/on_device_translation/public/supported_languages.h"
#include "components/optimization_guide/core/model_execution/on_device_model_download_progress_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/network_service_instance.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/on_device_model/public/mojom/download_observer.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif  // BUILDFLAG(IS_WIN)

namespace on_device_translation {
namespace {

base::FilePath GetFilePathFromGlobalPrefs(PrefService* prefs,
                                          std::string_view pref_name) {
  CHECK(prefs);
  base::FilePath path_in_pref = prefs->GetFilePath(pref_name);
  return path_in_pref;
}

const char kTranslateKitPackagePaths[] = "translate-kit-packages";

std::string GetLanguagePackComponentId(LanguagePackKey language_pack) {
  return crx_file::id_util::GenerateIdFromHash(
      GetLanguagePackComponentConfig(language_pack).public_key_sha);
}

// Runs `callback` asynchronously with whether the device has a network
// connection. A connection of unknown type counts as online.
void CheckOnline(base::OnceCallback<void(bool)> callback) {
  auto on_connection_type =
      [](base::OnceCallback<void(bool)> callback,
         net::NetworkChangeNotifier::ConnectionType type) {
        std::move(callback).Run(type !=
                                net::NetworkChangeNotifier::CONNECTION_NONE);
      };
  auto callbacks = base::SplitOnceCallback(std::move(callback));
  net::NetworkChangeNotifier::ConnectionType type;
  if (content::GetNetworkConnectionTracker()->GetConnectionType(
          &type,
          base::BindOnce(on_connection_type, std::move(callbacks.first)))) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(on_connection_type, std::move(callbacks.second), type));
  }
}

// Returns the available disk space on the component updater install volume, or
// `std::nullopt` if the directory or free space cannot be queried. Blocks.
std::optional<base::ByteSize> GetAvailableDiskSpace() {
  base::FilePath component_dir;
  if (!base::PathService::Get(component_updater::DIR_COMPONENT_USER,
                              &component_dir)) {
    return std::nullopt;
  }
  const std::optional<base::SysInfo::DiskSpaceInfo> disk_space =
      base::SysInfo::AmountOfDiskSpace(component_dir);
  if (!disk_space.has_value()) {
    return std::nullopt;
  }
  return disk_space->available;
}

std::optional<LanguagePackKey> FindLanguagePackKey(
    SupportedLanguage language1,
    SupportedLanguage language2) {
  for (auto it : kLanguagePackComponentConfigMap) {
    if (it.second->language1 == language1 &&
        it.second->language2 == language2) {
      return it.first;
    }
  }

  return std::nullopt;
}

std::optional<std::map<LanguagePackKey, base::FilePath>>
GetLanguagePackInfoFromCommandLine() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kTranslateKitPackagePaths)) {
    return std::nullopt;
  }
  const auto packages_string =
      command_line->GetSwitchValueNative(kTranslateKitPackagePaths);
  std::vector<base::CommandLine::StringType> splitted_strings =
      base::SplitString(packages_string,
#if BUILDFLAG(IS_WIN)
                        L",",
#else   // !BUILDFLAG(IS_WIN)
                        ",",
#endif  // BUILDFLAG(IS_WIN)
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (splitted_strings.size() % 3 != 0) {
    LOG(ERROR) << "Invalid --" << kTranslateKitPackagePaths << " flag.";
    return std::nullopt;
  }

  std::map<LanguagePackKey, base::FilePath> packages;
  auto it = splitted_strings.begin();
  while (it != splitted_strings.end()) {
    if (!base::IsStringASCII(*it) || !base::IsStringASCII(*(it + 1))) {
      LOG(ERROR) << "Invalid --" << kTranslateKitPackagePaths << " flag.";
      return std::nullopt;
    }
    std::string language1;
    std::string language2;
#if BUILDFLAG(IS_WIN)
    language1 = base::WideToUTF8(*(it++));
    language2 = base::WideToUTF8(*(it++));
#else  // !BUILDFLAG(IS_WIN)
    language1 = *(it++);
    language2 = *(it++);
#endif

    std::optional<SupportedLanguage> supported_language1 =
        ToSupportedLanguage(language1);
    std::optional<SupportedLanguage> supported_language2 =
        ToSupportedLanguage(language2);
    CHECK(supported_language1) << "Language not supported: " << language1;
    CHECK(supported_language2) << "Language not supported: " << language2;
    std::optional<LanguagePackKey> lpack_key =
        FindLanguagePackKey(*supported_language1, *supported_language2);
    if (!lpack_key) {
      LOG(ERROR) << "Language Pack not found " << language1 << "_" << language2;
      it++;
      continue;
    }
    packages.emplace(*lpack_key, base::FilePath(*(it++)));
  }
  return packages;
}

class ProgressTracker : public on_device_model::mojom::DownloadObserver {
 public:
  ProgressTracker(
      LanguagePackKey lang_pack,
      base::RepeatingCallback<void(LanguagePackKey, int)> callback,
      std::unique_ptr<optimization_guide::OnDeviceModelDownloadProgressManager>
          manager)
      : lang_pack_(lang_pack),
        callback_(callback),
        manager_(std::move(manager)) {
    manager_->AddObserver(receiver_.BindNewPipeAndPassRemote());
  }
  ~ProgressTracker() override = default;

  void OnDownloadProgressUpdate(uint64_t downloaded_bytes,
                                uint64_t total_bytes) override {
    int percentage = 0;
    if (total_bytes > 0) {
      percentage = static_cast<int>((downloaded_bytes * 100) / total_bytes);
    }
    callback_.Run(lang_pack_, percentage);
  }

 private:
  const LanguagePackKey lang_pack_;
  const base::RepeatingCallback<void(LanguagePackKey, int)> callback_;
  std::unique_ptr<optimization_guide::OnDeviceModelDownloadProgressManager>
      manager_;
  mojo::Receiver<on_device_model::mojom::DownloadObserver> receiver_{this};
};

}  // namespace

class OnDeviceTranslationInstallerImpl::Notifier {
 public:
  explicit Notifier(PrefService* local_state) {
    pref_registrar_.Init(local_state);
    // Start listening to pref changes for language pack keys.
    for (const auto& it : kLanguagePackComponentConfigMap) {
      pref_name_to_lang_pack_.emplace(GetComponentPathPrefName(*it.second),
                                      it.first);
      pref_registrar_.Add(
          GetComponentPathPrefName(*it.second),
          base::BindRepeating(&OnDeviceTranslationInstallerImpl::Notifier::
                                  NotifyLanguagePackInstallationChanged,
                              GetWeakPtr()));
    }
  }
  // Called when a language pack has finished being installed.
  void OnLanguagePackInstalled(LanguagePackKey language_pack) {
    for (Observer& observer : observers_) {
      observer.OnLanguagePackInstalled(language_pack);
    }
    StopProgressTracking(language_pack);
  }

  void OnLanguagePackProgress(LanguagePackKey language_pack, int progress) {
    for (Observer& observer : observers_) {
      observer.OnLanguagePackProgress(language_pack, progress);
    }
  }

  void StartProgressTracking(LanguagePackKey language_pack,
                             const base::flat_set<std::string>& component_ids) {
    auto manager = std::make_unique<
        optimization_guide::OnDeviceModelDownloadProgressManager>(
        g_browser_process->component_updater(), component_ids,
        /*enable_unloadable_progress=*/false);

    auto tracker = std::make_unique<ProgressTracker>(
        language_pack,
        base::BindRepeating(
            &OnDeviceTranslationInstallerImpl::Notifier::OnLanguagePackProgress,
            GetWeakPtr()),
        std::move(manager));

    installations_.emplace(language_pack, std::move(tracker));
  }

  void StopProgressTracking(LanguagePackKey language_pack) {
    auto it = installations_.find(language_pack);
    if (it != installations_.end()) {
      it->second.reset();
      installations_.erase(it);
    }
  }

  base::flat_map<LanguagePackKey, std::unique_ptr<ProgressTracker>>
      installations_;

  base::WeakPtr<OnDeviceTranslationInstallerImpl::Notifier> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void NotifyLanguagePackInstallationChanged(const std::string& pref_name) {
    const LanguagePackKey language_pack = pref_name_to_lang_pack_[pref_name];
    if (!pref_registrar_.prefs()->GetFilePath(pref_name).empty()) {
      // Components registered at startup have no install callback, so stop
      // progress tracking once the path pref becomes non-empty.
      StopProgressTracking(language_pack);
    }
    for (Observer& observer : observers_) {
      observer.OnLanguagePackInstallationChanged(language_pack);
    }
  }
  void NotifyInstallationChanged() {
    for (Observer& observer : observers_) {
      observer.OnInstallationChanged();
    }
  }
  void NotifyLibraryInstallFailed(InstallError error) {
    for (Observer& observer : observers_) {
      observer.OnLibraryInstallFailed(error);
    }
  }
  void NotifyLanguagePackInstallFailed(LanguagePackKey language_pack,
                                       InstallError error) {
    StopProgressTracking(language_pack);
    for (Observer& observer : observers_) {
      observer.OnLanguagePackInstallFailed(language_pack, error);
    }
  }
  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }
  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

 private:
  PrefChangeRegistrar pref_registrar_;
  base::ObserverList<Observer> observers_;
  base::flat_map<std::string, LanguagePackKey> pref_name_to_lang_pack_;
  base::WeakPtrFactory<OnDeviceTranslationInstallerImpl::Notifier>
      weak_ptr_factory_{this};
};

OnDeviceTranslationInstallerImpl::OnDeviceTranslationInstallerImpl()
    : OnDeviceTranslationInstallerImpl(TranslateKitLibrarySource::Create()) {}

OnDeviceTranslationInstallerImpl::OnDeviceTranslationInstallerImpl(
    std::unique_ptr<TranslateKitLibrarySource> library_source)
    : library_source_(std::move(library_source)),
      language_packs_from_command_line_(GetLanguagePackInfoFromCommandLine()) {
  CHECK(library_source_);
  notifier_ = std::make_unique<OnDeviceTranslationInstallerImpl::Notifier>(
      g_browser_process->local_state());
  library_source_->SetClient(this);
}

OnDeviceTranslationInstallerImpl::~OnDeviceTranslationInstallerImpl() = default;

base::FilePath OnDeviceTranslationInstallerImpl::GetLibraryPath() const {
  CHECK(IsInit()) << "Trying to use the OnDeviceTranslationInstaller before "
                     "initializing it.";
  return library_source_->GetLibraryPath();
}

base::FilePath OnDeviceTranslationInstallerImpl::GetLanguagePackPath(
    LanguagePackKey language_pack) const {
  CHECK(IsInit()) << "Trying to use the OnDeviceTranslationInstaller before "
                     "initializing it.";
  if (language_packs_from_command_line_.has_value()) {
    if (!language_packs_from_command_line_->contains(language_pack)) {
      return base::FilePath();
    }

    return (*language_packs_from_command_line_).at(language_pack);
  }

  const auto* config =
      on_device_translation::kLanguagePackComponentConfigMap.at(language_pack);
  return g_browser_process->local_state()->GetFilePath(
      on_device_translation::GetComponentPathPrefName(*config));
}

std::set<LanguagePackKey>
OnDeviceTranslationInstallerImpl::GetLanguagePackKeysFromCommandLine() const {
  std::set<LanguagePackKey> keys;
  std::transform(language_packs_from_command_line_->begin(),
                 language_packs_from_command_line_->end(),
                 std::inserter(keys, keys.begin()),
                 [](const auto& pair) { return pair.first; });
  return keys;
}

std::set<LanguagePackKey>
OnDeviceTranslationInstallerImpl::InstalledLanguagePacks() const {
  if (language_packs_from_command_line_.has_value()) {
    return GetLanguagePackKeysFromCommandLine();
  }

  std::set<LanguagePackKey> installed_pack_keys;
  for (const auto& it : kLanguagePackComponentConfigMap) {
    if (!GetFilePathFromGlobalPrefs(g_browser_process->local_state(),
                                    GetComponentPathPrefName(*it.second))
             .empty()) {
      installed_pack_keys.insert(it.first);
    }
  }
  return installed_pack_keys;
}

std::set<LanguagePackKey>
OnDeviceTranslationInstallerImpl::RegisteredLanguagePacks() const {
  if (language_packs_from_command_line_.has_value()) {
    return GetLanguagePackKeysFromCommandLine();
  }

  // Language packs that wait for the checks before the download count as
  // registered.
  std::set<LanguagePackKey> registered_pack_keys =
      language_pack_checks_in_progress_;
  for (const auto& it : kLanguagePackComponentConfigMap) {
    if (g_browser_process->local_state()->GetBoolean(
            GetRegisteredFlagPrefName(*it.second))) {
      registered_pack_keys.insert(it.first);
    }
  }
  return registered_pack_keys;
}

bool OnDeviceTranslationInstallerImpl::IsSupported() const {
#if BUILDFLAG(IS_ANDROID)
  // Automotive devices have tight memory and storage limits for on-device
  // translation. Holds when a Finch trial or command-line flag enables
  // `kTranslationAPI` over the default disable override in
  // `ChromeBrowserFieldTrials`.
  if (base::android::device_info::is_automotive()) {
    return false;
  }
#endif  // BUILDFLAG(IS_ANDROID)
  const size_t min_physical_memory_mb =
      kTranslationAPIMinPhysicalMemoryMB.Get();
  if (min_physical_memory_mb > 0 &&
      base::SysInfo::AmountOfTotalPhysicalMemory() <
          base::MiB(min_physical_memory_mb)) {
    return false;
  }
  return library_source_->IsSupported();
}

bool OnDeviceTranslationInstallerImpl::IsInit() const {
  return !library_source_->GetLibraryPath().empty();
}

void OnDeviceTranslationInstallerImpl::Init(
    base::RepeatingClosure on_ready_callback) {
  if (IsInit()) {
    if (on_ready_callback) {
      on_ready_callback.Run();
    }
    return;
  }
  if (!IsSupported()) {
    // The library can't be installed, so the callback would never run.
    FailLibraryInstall(InstallError::kFailed);
    return;
  }
  // Callers that only start the install pass a null callback, so that repeated
  // calls don't queue callbacks.
  if (on_ready_callback) {
    on_ready_callbacks_.push_back(std::move(on_ready_callback));
  }
  if (library_install_in_progress_) {
    // The callback waits for the install that runs. Don't check the connection
    // again: a short offline moment, for example a network handover, must not
    // fail an install that continues. Still ask the source, which ignores the
    // request while its install runs. The source doesn't report every end of
    // an install, for example when the periodic update check ran at the same
    // time, so this request is how the install starts again. Until the source
    // reports the end of an install, an offline device gets the error of the
    // source instead of `kOffline` at once.
    library_source_->Install();
    return;
  }
  if (library_check_in_progress_) {
    return;
  }
  library_check_in_progress_ = true;
  CheckOnline(
      base::BindOnce(&OnDeviceTranslationInstallerImpl::OnLibraryOnlineChecked,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OnDeviceTranslationInstallerImpl::OnLibraryOnlineChecked(bool online) {
  library_check_in_progress_ = false;
  if (IsInit() || library_install_in_progress_) {
    return;
  }
  if (!online) {
    FailLibraryInstall(InstallError::kOffline);
    return;
  }
  if (IsMeteredDownloadBlocked()) {
    FailLibraryInstall(InstallError::kMeteredConnectionBlocked);
    return;
  }
  library_install_in_progress_ = true;
  library_source_->Install();
}

void OnDeviceTranslationInstallerImpl::InstallLanguagePack(
    LanguagePackKey language_pack) {
  if (language_packs_from_command_line_.has_value()) {
    if (!language_packs_from_command_line_->contains(language_pack)) {
      LOG(ERROR) << "Language pack not found in the command line.";
      return;
    }

    notifier_->OnLanguagePackInstalled(language_pack);
    return;
  }

  if (language_pack_updates_in_progress_.contains(language_pack)) {
    // A download of the pack runs. Don't check the connection again: a short
    // offline moment, for example a network handover, must not fail a download
    // that continues.
    return;
  }
  if (!language_pack_checks_in_progress_.insert(language_pack).second) {
    return;
  }
  CheckOnline(base::BindOnce(
      &OnDeviceTranslationInstallerImpl::OnLanguagePackOnlineChecked,
      weak_ptr_factory_.GetWeakPtr(), language_pack));
}

void OnDeviceTranslationInstallerImpl::OnLanguagePackOnlineChecked(
    LanguagePackKey language_pack,
    bool online) {
  std::optional<InstallError> error;
  if (!online) {
    error = InstallError::kOffline;
  } else if (data_saver::IsDataSaverEnabled()) {
    error = InstallError::kDataSaverEnabled;
  } else if (IsMeteredDownloadBlocked()) {
    error = InstallError::kMeteredConnectionBlocked;
  }
  if (error) {
    language_pack_checks_in_progress_.erase(language_pack);
    if (!InstalledLanguagePacks().contains(language_pack)) {
      notifier_->NotifyLanguagePackInstallFailed(language_pack, *error);
    }
    return;
  }
  const size_t min_free_disk_space_mb =
      kTranslationAPILanguagePackMinFreeDiskSpaceMB.Get();
  if (min_free_disk_space_mb == 0) {
    language_pack_checks_in_progress_.erase(language_pack);
    DownloadLanguagePack(language_pack);
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GetAvailableDiskSpace),
      base::BindOnce(
          &OnDeviceTranslationInstallerImpl::OnLanguagePackDiskSpaceChecked,
          weak_ptr_factory_.GetWeakPtr(), language_pack,
          min_free_disk_space_mb));
}

void OnDeviceTranslationInstallerImpl::OnLanguagePackDiskSpaceChecked(
    LanguagePackKey language_pack,
    size_t min_free_disk_space_mb,
    std::optional<base::ByteSize> available) {
  language_pack_checks_in_progress_.erase(language_pack);
  // Don't block the download if the free space is unknown.
  if (available.has_value() && available->InMiB() < min_free_disk_space_mb) {
    notifier_->NotifyLanguagePackInstallFailed(
        language_pack, InstallError::kNotEnoughDiskSpace);
    return;
  }
  DownloadLanguagePack(language_pack);
}

void OnDeviceTranslationInstallerImpl::DownloadLanguagePack(
    LanguagePackKey language_pack) {
  // Calculate component IDs to track progress
  const std::string component_id = GetLanguagePackComponentId(language_pack);

  component_updater::ComponentUpdateService* cus =
      g_browser_process->component_updater();
  if (std::ranges::contains(cus->GetComponentIDs(), component_id)) {
    // The component was registered before, for example at startup. If it
    // isn't installed, ask for it now instead of waiting for the next periodic
    // update check. If it is installed, nothing would stop the progress
    // tracking, so don't start it.
    if (!InstalledLanguagePacks().contains(language_pack)) {
      notifier_->StartProgressTracking(language_pack, {component_id});
      UpdateLanguagePackOnDemand(language_pack);
    }
    return;
  }

  if (language_pack_updates_in_progress_.contains(language_pack)) {
    return;
  }

  // Registers the TranslateKit language pack component.
  RecordLanguagePairUma("Translate.OnDeviceTranslation.Download.LanguagePair",
                        GetSourceLanguageCode(language_pack),
                        GetTargetLanguageCode(language_pack));
  notifier_->StartProgressTracking(language_pack, {component_id});
  language_pack_updates_in_progress_.insert(language_pack);
  component_updater::RegisterTranslateKitLanguagePackComponent(
      cus, g_browser_process->local_state(), language_pack,
      base::BindOnce(
          [](base::WeakPtr<OnDeviceTranslationInstallerImpl> installer,
             LanguagePackKey language_pack) {
            if (installer) {
              installer->language_pack_updates_in_progress_.erase(
                  language_pack);
              installer->UpdateLanguagePackOnDemand(language_pack);
            }
          },
          weak_ptr_factory_.GetWeakPtr(), language_pack),
      base::BindRepeating(
          &OnDeviceTranslationInstallerImpl::Notifier::OnLanguagePackInstalled,
          notifier_->GetWeakPtr(), language_pack));
}

void OnDeviceTranslationInstallerImpl::UpdateLanguagePackOnDemand(
    LanguagePackKey language_pack) {
  if (!language_pack_updates_in_progress_.insert(language_pack).second) {
    return;
  }
  component_updater::TranslateKitLanguagePackComponentInstallerPolicy::
      UpdateComponentOnDemand(
          g_browser_process->component_updater(), language_pack,
          base::BindOnce(
              &OnDeviceTranslationInstallerImpl::OnLanguagePackUpdateFinished,
              weak_ptr_factory_.GetWeakPtr(), language_pack));
}

void OnDeviceTranslationInstallerImpl::OnLanguagePackUpdateFinished(
    LanguagePackKey language_pack,
    update_client::Error error) {
  language_pack_updates_in_progress_.erase(language_pack);
  std::optional<InstallError> install_error =
      GetOnDemandUpdateError(g_browser_process->component_updater(),
                             GetLanguagePackComponentId(language_pack), error,
                             InstalledLanguagePacks().contains(language_pack));
  if (install_error) {
    notifier_->NotifyLanguagePackInstallFailed(language_pack, *install_error);
  }
}

void OnDeviceTranslationInstallerImpl::UnInstallLanguagePack(
    LanguagePackKey language_pack) {
  // Uninstalls the TranslateKit language pack component.
  component_updater::UninstallTranslateKitLanguagePackComponent(
      g_browser_process->component_updater(), g_browser_process->local_state(),
      language_pack);
}

void OnDeviceTranslationInstallerImpl::AddObserver(Observer* observer) {
  notifier_->AddObserver(observer);
}

void OnDeviceTranslationInstallerImpl::RemoveObserver(Observer* observer) {
  notifier_->RemoveObserver(observer);
}

void OnDeviceTranslationInstallerImpl::OnLibraryChanged() {
  if (IsInit()) {
    library_install_in_progress_ = false;
    std::vector<base::RepeatingClosure> callbacks =
        std::exchange(on_ready_callbacks_, {});
    for (auto& callback : callbacks) {
      callback.Run();
    }
  }
  notifier_->NotifyInstallationChanged();
}

void OnDeviceTranslationInstallerImpl::OnLibraryInstallFailed(
    InstallError error) {
  FailLibraryInstall(error);
}

void OnDeviceTranslationInstallerImpl::FailLibraryInstall(InstallError error) {
  library_install_in_progress_ = false;
  if (IsInit()) {
    return;
  }
  // The callbacks never run. The next `Init()` call retries the install.
  on_ready_callbacks_.clear();
  notifier_->NotifyLibraryInstallFailed(error);
}

}  // namespace on_device_translation
