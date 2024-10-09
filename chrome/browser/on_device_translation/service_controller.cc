// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/service_controller.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/translate_kit_component_installer.h"
#include "chrome/browser/component_updater/translate_kit_language_pack_component_installer.h"
#include "chrome/browser/on_device_translation/constants.h"
#include "chrome/browser/on_device_translation/language_pack_util.h"
#include "chrome/browser/on_device_translation/pref_names.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/prefs/pref_service.h"
#include "components/services/on_device_translation/public/cpp/features.h"
#include "components/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/services/on_device_translation/public/mojom/translator.mojom.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_host_passkeys.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif  // BUILDFLAG(IS_WIN)

using on_device_translation::kLanguagePackComponentConfigMap;
using on_device_translation::LanguagePackKey;
using on_device_translation::ToLanguageCode;
using on_device_translation::mojom::FileOperationProxy;
using on_device_translation::mojom::OnDeviceTranslationLanguagePackage;
using on_device_translation::mojom::OnDeviceTranslationLanguagePackagePtr;
using on_device_translation::mojom::OnDeviceTranslationServiceConfig;
using on_device_translation::mojom::OnDeviceTranslationServiceConfigPtr;

namespace {

constexpr size_t kMaxPendingTaskCount = 1024;

const char kTranslateKitPackagePaths[] = "translate-kit-packages";

const char kOnDeviceTranslationServiceDisplayName[] =
    "On-device Translation Service";

base::FilePath GetFilePathFromGlobalPrefs(std::string_view pref_name) {
  PrefService* global_prefs = g_browser_process->local_state();
  CHECK(global_prefs);
  base::FilePath path_in_pref = global_prefs->GetFilePath(pref_name);
  return path_in_pref;
}

base::FilePath GetTranslateKitLibraryPath() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(on_device_translation::kTranslateKitBinaryPath)) {
    return command_line->GetSwitchValuePath(
        on_device_translation::kTranslateKitBinaryPath);
  }
  return GetFilePathFromGlobalPrefs(prefs::kTranslateKitBinaryPath);
}

std::string ToString(base::FilePath path) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/362123222): Get rid of conditional decoding.
  return path.AsUTF8Unsafe();
#else
  return path.value();
#endif  // BUILDFLAG(IS_WIN)
}

// Returns the language packs that are installed.
std::set<LanguagePackKey> GetInstalledLanguagePacks() {
  std::set<LanguagePackKey> insalled_pack_keys;
  for (const auto& it : kLanguagePackComponentConfigMap) {
    if (!GetFilePathFromGlobalPrefs(it.second->config_path_pref).empty()) {
      insalled_pack_keys.insert(it.first);
    }
  }
  return insalled_pack_keys;
}

}  // namespace

OnDeviceTranslationServiceController::PendingTask::PendingTask(
    std::set<on_device_translation::LanguagePackKey> required_packs,
    base::OnceClosure once_closure)
    : required_packs(std::move(required_packs)),
      once_closure(std::move(once_closure)) {}

OnDeviceTranslationServiceController::PendingTask::~PendingTask() = default;
OnDeviceTranslationServiceController::PendingTask::PendingTask(PendingTask&&) =
    default;
OnDeviceTranslationServiceController::PendingTask&
OnDeviceTranslationServiceController::PendingTask::operator=(PendingTask&&) =
    default;

// Implementation of FileOperationProxy. It is used to provide file operations
// to the OnDeviceTranslationService. This is created on the UI thread and
// destroyed on the background thread of the passed `task_runner`.
class OnDeviceTranslationServiceController::FileOperationProxyImpl
    : public FileOperationProxy {
 public:
  FileOperationProxyImpl(
      mojo::PendingReceiver<FileOperationProxy> proxy_receiver,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::vector<base::FilePath> package_pathes)
      : receiver_(this, std::move(proxy_receiver), task_runner),
        package_pathes_(std::move(package_pathes)) {}
  ~FileOperationProxyImpl() override = default;

  // FileOperationProxy implementation:
  void FileExists(uint32_t package_index,
                  const base::FilePath& relative_path,
                  FileExistsCallback callback) override {
    const base::FilePath file_path = GetFilePath(package_index, relative_path);
    if (file_path.empty()) {
      // Invalid `path` was passed.
      std::move(callback).Run(/*exists=*/false, /*is_directory=*/false);
      return;
    }
    if (!base::PathExists(file_path)) {
      // File doesn't exist.
      std::move(callback).Run(/*exists=*/false, /*is_directory=*/false);
      return;
    }
    std::move(callback).Run(
        /*exists=*/true,
        /*is_directory=*/base::DirectoryExists(file_path));
  }
  void Open(uint32_t package_index,
            const base::FilePath& relative_path,
            OpenCallback callback) override {
    const base::FilePath file_path = GetFilePath(package_index, relative_path);
    std::move(callback).Run(
        file_path.empty() ? base::File()
                          : base::File(file_path, base::File::FLAG_OPEN |
                                                      base::File::FLAG_READ));
  }

 private:
  base::FilePath GetFilePath(uint32_t package_index,
                             const base::FilePath& relative_path) {
    if (package_index >= package_pathes_.size()) {
      // Invalid package index.
      return base::FilePath();
    }
    if (relative_path.IsAbsolute() || relative_path.ReferencesParent()) {
      // Invalid relative path.
      return base::FilePath();
    }
    return package_pathes_[package_index].Append(relative_path);
  }

  mojo::Receiver<FileOperationProxy> receiver_{this};
  std::vector<base::FilePath> package_pathes_;
};

// static
base::FilePath
OnDeviceTranslationServiceController::GetTranslateKitComponentPath() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(on_device_translation::kTranslateKitBinaryPath)) {
    return command_line->GetSwitchValuePath(
        on_device_translation::kTranslateKitBinaryPath);
  }
  base::FilePath components_dir;
  base::PathService::Get(component_updater::DIR_COMPONENT_USER,
                         &components_dir);
  return components_dir.empty()
             ? base::FilePath()
             : components_dir.Append(
                   on_device_translation::
                       kTranslateKitBinaryInstallationRelativeDir);
}

std::optional<
    std::vector<OnDeviceTranslationServiceController::LanguagePackInfo>>
OnDeviceTranslationServiceController::GetLanguagePackInfoFromCommandLine() {
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
    LOG(ERROR) << "Invalid --translate-kit-packages flag";
    return std::nullopt;
  }

  std::vector<OnDeviceTranslationServiceController::LanguagePackInfo> packages;
  auto it = splitted_strings.begin();
  while (it != splitted_strings.end()) {
    if (!base::IsStringASCII(*it) || !base::IsStringASCII(*(it + 1))) {
      LOG(ERROR) << "Invalid --translate-kit-packages flag";
      return std::nullopt;
    }
    OnDeviceTranslationServiceController::LanguagePackInfo package;
#if BUILDFLAG(IS_WIN)
    package.language1 = base::WideToUTF8(*(it++));
    package.language2 = base::WideToUTF8(*(it++));
#else  // !BUILDFLAG(IS_WIN)
    package.language1 = *(it++);
    package.language2 = *(it++);
#endif
    package.package_path = base::FilePath(*(it++));
    packages.push_back(std::move(package));
  }
  return packages;
}

OnDeviceTranslationServiceController::OnDeviceTranslationServiceController()
    : language_packs_from_command_line_(GetLanguagePackInfoFromCommandLine()),
      file_operation_proxy_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {
  // Initialize the pref change registrar.
  pref_change_registrar_.Init(g_browser_process->local_state());
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          on_device_translation::kTranslateKitBinaryPath)) {
    // Start listening to pref changes for TranslateKit binary path.
    pref_change_registrar_.Add(
        prefs::kTranslateKitBinaryPath,
        base::BindRepeating(&OnDeviceTranslationServiceController::
                                OnTranslateKitBinaryPathChanged,
                            base::Unretained(this)));
    // Registers the TranslateKit component.
    component_updater::RegisterTranslateKitComponent(
        g_browser_process->component_updater(),
        g_browser_process->local_state());
  }
  if (!language_packs_from_command_line_) {
    // Start listening to pref changes for language pack keys.
    for (const auto& it : kLanguagePackComponentConfigMap) {
      pref_change_registrar_.Add(
          it.second->config_path_pref,
          base::BindRepeating(&OnDeviceTranslationServiceController::
                                  OnLanguagePackKeyPrefChanged,
                              base::Unretained(this)));
    }
    // Register all the installed language pack components.
    RegisterInstalledLanguagePackComponent();
  }
}

OnDeviceTranslationServiceController::~OnDeviceTranslationServiceController() =
    default;

void OnDeviceTranslationServiceController::CreateTranslator(
    const std::string& source_lang,
    const std::string& target_lang,
    mojo::PendingReceiver<on_device_translation::mojom::Translator> receiver,
    base::OnceCallback<void(bool)> callback) {
  std::set<LanguagePackKey> required_packs;
  std::vector<LanguagePackKey> required_not_installed_packs;
  // If the language packs are set by the command line, we don't need to check
  // the installed language packs.
  if (!language_packs_from_command_line_) {
    std::vector<LanguagePackKey> to_be_downloaded_packs;
    CalculateLanguagePackRequirements(source_lang, target_lang, required_packs,
                                      required_not_installed_packs,
                                      to_be_downloaded_packs);
    for (const auto& language_pack : to_be_downloaded_packs) {
      // Register the language pack component.
      RegisterLanguagePackComponent(language_pack);
    }
  }
  // If there is no TranslteKit or there are required language packs that are
  // not installed, we will wait until they are installed to create the
  // translator.
  if (GetTranslateKitLibraryPath().empty() ||
      !required_not_installed_packs.empty()) {
    // When the size of pending tasks is too large, we will not queue the new
    // task and hadle the request as failure to avoid OOM of the browser
    // process.
    if (pending_tasks_.size() == kMaxPendingTaskCount) {
      std::move(callback).Run(false);
      return;
    }
    pending_tasks_.emplace_back(
        required_packs,
        base::BindOnce(
            &OnDeviceTranslationServiceController::CreateTranslatorImpl,
            base::Unretained(this), source_lang, target_lang,
            std::move(receiver), std::move(callback)));
    return;
  }
  CreateTranslatorImpl(source_lang, target_lang, std::move(receiver),
                       std::move(callback));
}

void OnDeviceTranslationServiceController::CreateTranslatorImpl(
    const std::string& source_lang,
    const std::string& target_lang,
    mojo::PendingReceiver<on_device_translation::mojom::Translator> receiver,
    base::OnceCallback<void(bool)> callback) {
  GetRemote()->CreateTranslator(source_lang, target_lang, std::move(receiver),
                                std::move(callback));
}

void OnDeviceTranslationServiceController::CanTranslate(
    const std::string& source_lang,
    const std::string& target_lang,
    base::OnceCallback<void(bool)> callback) {
  // If there is no TranslteKit, we immediately return false.
  // TODO(crbug.com/331735396): Support "after-download" capabilities.
  if (GetTranslateKitLibraryPath().empty()) {
    std::move(callback).Run(false);
    return;
  }
  GetRemote()->CanTranslate(source_lang, target_lang, std::move(callback));
}

// Returns the language packs that are installed or set by the command line.
std::vector<OnDeviceTranslationServiceController::LanguagePackInfo>
OnDeviceTranslationServiceController::GetLanguagePackInfo() {
  if (language_packs_from_command_line_) {
    return *language_packs_from_command_line_;
  }

  std::vector<OnDeviceTranslationServiceController::LanguagePackInfo> packages;
  for (const auto& it : kLanguagePackComponentConfigMap) {
    auto file_path = GetFilePathFromGlobalPrefs(it.second->config_path_pref);
    if (!file_path.empty()) {
      OnDeviceTranslationServiceController::LanguagePackInfo package;
      package.language1 = ToLanguageCode(it.second->language1);
      package.language2 = ToLanguageCode(it.second->language2);
      package.package_path = file_path;
      packages.push_back(std::move(package));
    }
  }
  return packages;
}

// Register the installed language pack components.
void OnDeviceTranslationServiceController::
    RegisterInstalledLanguagePackComponent() {
  for (const auto& language_pack : GetInstalledLanguagePacks()) {
    RegisterLanguagePackComponent(language_pack);
  }
}

// Register the language pack component.
void OnDeviceTranslationServiceController::RegisterLanguagePackComponent(
    LanguagePackKey language_pack) {
  CHECK(!registered_language_packs_.contains(language_pack));
  registered_language_packs_.insert(language_pack);
  component_updater::RegisterTranslateKitLanguagePackComponent(
      g_browser_process->component_updater(), g_browser_process->local_state(),
      language_pack, base::BindOnce([]() {
        // TODO(crbug.com/358030919): Consider calling
        // OnDemandUpdater::OnDemandUpdate() to trigger an update check.
      }));
}

// Called when the TranslateKitBinaryPath pref is changed.
void OnDeviceTranslationServiceController::OnTranslateKitBinaryPathChanged(
    const std::string& pref_name) {
  service_remote_.reset();
  MaybeRunPendingTasks();
}

// Called when the language pack key pref is changed.
void OnDeviceTranslationServiceController::OnLanguagePackKeyPrefChanged(
    const std::string& pref_name) {
  service_remote_.reset();
  MaybeRunPendingTasks();
}

void OnDeviceTranslationServiceController::MaybeRunPendingTasks() {
  if (pending_tasks_.empty()) {
    return;
  }
  if (GetTranslateKitLibraryPath().empty()) {
    return;
  }
  const auto installed_packs = GetInstalledLanguagePacks();
  std::vector<PendingTask> pending_tasks = std::move(pending_tasks_);
  for (auto& task : pending_tasks) {
    if (base::ranges::all_of(task.required_packs.begin(),
                             task.required_packs.end(),
                             [&](const LanguagePackKey& key) {
                               return installed_packs.contains(key);
                             })) {
      std::move(task.once_closure).Run();
    } else {
      pending_tasks_.push_back(std::move(task));
    }
  }
}

mojo::Remote<on_device_translation::mojom::OnDeviceTranslationService>&
OnDeviceTranslationServiceController::GetRemote() {
  if (service_remote_) {
    return service_remote_;
  }

  auto receiver = service_remote_.BindNewPipeAndPassReceiver();
  service_remote_.reset_on_disconnect();

  const base::FilePath binary_path = GetTranslateKitLibraryPath();
  CHECK(!binary_path.empty())
      << "Got an empty path to TranslateKit binary on the device.";

  std::vector<std::string> extra_switches;
  extra_switches.push_back(
      base::StrCat({on_device_translation::kTranslateKitBinaryPath, "=",
                    ToString(binary_path)}));

  content::ServiceProcessHost::Launch<
      on_device_translation::mojom::OnDeviceTranslationService>(
      std::move(receiver),
      content::ServiceProcessHost::Options()
          .WithDisplayName(kOnDeviceTranslationServiceDisplayName)
          .WithExtraCommandLineSwitches(extra_switches)
#if BUILDFLAG(IS_WIN)
          .WithPreloadedLibraries(
              {binary_path},
              content::ServiceProcessHostPreloadLibraries::GetPassKey())
#endif
          .Pass());

  const auto packages = GetLanguagePackInfo();
  auto config = OnDeviceTranslationServiceConfig::New();
  std::vector<base::FilePath> package_pathes;
  for (const auto& package : packages) {
    auto mojo_package = OnDeviceTranslationLanguagePackage::New();
    mojo_package->language1 = package.language1;
    mojo_package->language2 = package.language2;
    config->packages.push_back(std::move(mojo_package));
    package_pathes.push_back(package.package_path);
  }
  mojo::PendingReceiver<FileOperationProxy> proxy_receiver =
      config->file_operation_proxy.InitWithNewPipeAndPassReceiver();
  service_remote_->SetServiceConfig(std::move(config));

  // Create a task runner to run the FileOperationProxy.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  // Create the FileOperationProxy which lives in the background thread of
  // `task_runner`.
  file_operation_proxy_ =
      std::unique_ptr<FileOperationProxyImpl, base::OnTaskRunnerDeleter>(
          new FileOperationProxyImpl(std::move(proxy_receiver), task_runner,
                                     std::move(package_pathes)),
          base::OnTaskRunnerDeleter(task_runner));
  return service_remote_;
}

void OnDeviceTranslationServiceController::CalculateLanguagePackRequirements(
    const std::string& source_lang,
    const std::string& target_lang,
    std::set<LanguagePackKey>& required_packs,
    std::vector<LanguagePackKey>& required_not_installed_packs,
    std::vector<LanguagePackKey>& to_be_downloaded_packs) {
  CHECK(required_packs.empty());
  CHECK(required_not_installed_packs.empty());
  CHECK(to_be_downloaded_packs.empty());
  required_packs = on_device_translation::CalculateRequiredLanguagePacks(
      source_lang, target_lang);
  const auto installed_packs = GetInstalledLanguagePacks();
  base::ranges::set_difference(
      required_packs, installed_packs,
      std::back_inserter(required_not_installed_packs));
  base::ranges::set_difference(required_not_installed_packs,
                               registered_language_packs_,
                               std::back_inserter(to_be_downloaded_packs));
}

// static
OnDeviceTranslationServiceController*
OnDeviceTranslationServiceController::GetInstance() {
  static base::NoDestructor<OnDeviceTranslationServiceController> instance;
  return instance.get();
}
