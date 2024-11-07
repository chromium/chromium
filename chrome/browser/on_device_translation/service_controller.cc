// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/service_controller.h"

#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
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
#include "chrome/browser/on_device_translation/component_manager.h"
#include "chrome/browser/on_device_translation/constants.h"
#include "chrome/browser/on_device_translation/file_operation_proxy_impl.h"
#include "chrome/browser/on_device_translation/language_pack_util.h"
#include "chrome/browser/on_device_translation/pref_names.h"
#include "chrome/browser/on_device_translation/translation_metrics.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/services/on_device_translation/public/cpp/features.h"
#include "components/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/services/on_device_translation/public/mojom/translator.mojom.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_host_passkeys.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom-shared.h"

using blink::mojom::CanCreateTranslatorResult;

namespace on_device_translation {

using mojom::FileOperationProxy;
using mojom::OnDeviceTranslationLanguagePackage;
using mojom::OnDeviceTranslationLanguagePackagePtr;
using mojom::OnDeviceTranslationServiceConfig;
using mojom::OnDeviceTranslationServiceConfigPtr;

namespace {

// Limit the number of downloadable language packs to 3 during OT to mitigate
// the risk of fingerprinting attacks.
constexpr size_t kTranslationAPILimitLanguagePackCountMax = 3;

const char kOnDeviceTranslationServiceDisplayName[] =
    "On-device Translation Service";

std::string ToString(base::FilePath path) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/362123222): Get rid of conditional decoding.
  return path.AsUTF8Unsafe();
#else
  return path.value();
#endif  // BUILDFLAG(IS_WIN)
}

}  // namespace

OnDeviceTranslationServiceController::PendingTask::PendingTask(
    std::set<LanguagePackKey> required_packs,
    base::OnceClosure once_closure)
    : required_packs(std::move(required_packs)),
      once_closure(std::move(once_closure)) {}

OnDeviceTranslationServiceController::PendingTask::~PendingTask() = default;
OnDeviceTranslationServiceController::PendingTask::PendingTask(PendingTask&&) =
    default;
OnDeviceTranslationServiceController::PendingTask&
OnDeviceTranslationServiceController::PendingTask::operator=(PendingTask&&) =
    default;

OnDeviceTranslationServiceController::OnDeviceTranslationServiceController()
    : file_operation_proxy_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {
  // Initialize the pref change registrar.
  pref_change_registrar_.Init(g_browser_process->local_state());
  if (!ComponentManager::HasTranslateKitLibraryPathFromCommandLine()) {
    // Start listening to pref changes for TranslateKit binary path.
    pref_change_registrar_.Add(
        prefs::kTranslateKitBinaryPath,
        base::BindRepeating(&OnDeviceTranslationServiceController::
                                OnTranslateKitBinaryPathChanged,
                            base::Unretained(this)));
    // Registers the TranslateKit component.
    ComponentManager::GetInstance().RegisterTranslateKitComponent();
  }
  if (!ComponentManager::GetInstance().HasLanguagePackInfoFromCommandLine()) {
    // Start listening to pref changes for language pack keys.
    for (const auto& it : kLanguagePackComponentConfigMap) {
      pref_change_registrar_.Add(
          GetComponentPathPrefName(*it.second),
          base::BindRepeating(&OnDeviceTranslationServiceController::
                                  OnLanguagePackKeyPrefChanged,
                              base::Unretained(this)));
    }
  }
}

OnDeviceTranslationServiceController::~OnDeviceTranslationServiceController() =
    default;

void OnDeviceTranslationServiceController::CreateTranslator(
    const std::string& source_lang,
    const std::string& target_lang,
    base::OnceCallback<void(mojo::PendingRemote<mojom::Translator>)> callback) {
  std::set<LanguagePackKey> required_packs;
  std::vector<LanguagePackKey> required_not_installed_packs;
  // If the language packs are set by the command line, we don't need to check
  // the installed language packs.
  if (!ComponentManager::GetInstance().HasLanguagePackInfoFromCommandLine()) {
    std::vector<LanguagePackKey> to_be_registered_packs;
    CalculateLanguagePackRequirements(source_lang, target_lang, required_packs,
                                      required_not_installed_packs,
                                      to_be_registered_packs);
    if (!to_be_registered_packs.empty()) {
      if (kTranslationAPILimitLanguagePackCount.Get()) {
        if (to_be_registered_packs.size() +
                ComponentManager::GetRegisteredLanguagePacks().size() >
            kTranslationAPILimitLanguagePackCountMax) {
          // TODO(crbug.com/358030919): Consider printing errors
          // to DevTool's console.
          RecordLanguagePairUma(
              "Translate.OnDeviceTranslation.DownloadExceedLimit.LanguagePair",
              source_lang, target_lang);
          std::move(callback).Run(mojo::NullRemote());
          return;
        }
      }

      for (const auto& language_pack : to_be_registered_packs) {
        RecordLanguagePairUma(
            "Translate.OnDeviceTranslation.Download.LanguagePair",
            GetSourceLanguageCode(language_pack),
            GetTargetLanguageCode(language_pack));
        // Register the language pack component.
        ComponentManager::GetInstance()
            .RegisterTranslateKitLanguagePackComponent(language_pack);
      }
    }
  }
  // If there is no TranslteKit or there are required language packs that are
  // not installed, we will wait until they are installed to create the
  // translator.
  if (ComponentManager::GetTranslateKitLibraryPath().empty() ||
      !required_not_installed_packs.empty()) {
    // When the size of pending tasks is too large, we will not queue the new
    // task and hadle the request as failure to avoid OOM of the browser
    // process.
    if (pending_tasks_.size() == kMaxPendingTaskCount) {
      std::move(callback).Run(mojo::NullRemote());
      return;
    }
    pending_tasks_.emplace_back(
        required_packs,
        base::BindOnce(
            &OnDeviceTranslationServiceController::CreateTranslatorImpl,
            base::Unretained(this), source_lang, target_lang,
            std::move(callback)));
    return;
  }
  CreateTranslatorImpl(source_lang, target_lang, std::move(callback));
}

void OnDeviceTranslationServiceController::CreateTranslatorImpl(
    const std::string& source_lang,
    const std::string& target_lang,
    base::OnceCallback<void(mojo::PendingRemote<mojom::Translator>)> callback) {
  GetRemote()->CreateTranslator(source_lang, target_lang,
                                mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                                    std::move(callback), mojo::NullRemote()));
}

void OnDeviceTranslationServiceController::CanTranslate(
    const std::string& source_lang,
    const std::string& target_lang,
    base::OnceCallback<void(CanCreateTranslatorResult)> callback) {
  if (!ComponentManager::GetInstance().HasLanguagePackInfoFromCommandLine()) {
    // If the language packs are not set by the command line, returns the result
    // of CanTranslateImpl().
    std::move(callback).Run(CanTranslateImpl(source_lang, target_lang));
    return;
  }
  // Otherwise, checks the availability of the library and ask the on device
  // translation service.
  if (ComponentManager::GetTranslateKitLibraryPath().empty()) {
    // Note: Strictly saying, returning AfterDownloadLibraryNotReady is not
    // correct. It might happen that the language packs are missing. But it is
    // OK because this only impacts people loading packs from the commandline.
    std::move(callback).Run(
        CanCreateTranslatorResult::kAfterDownloadLibraryNotReady);
    return;
  }

  auto callbacks = base::SplitOnceCallback(std::move(callback));
  GetRemote()->CanTranslate(
      source_lang, target_lang,
      mojo::WrapCallbackWithDropHandler(
          base::BindOnce(
              [](base::OnceCallback<void(CanCreateTranslatorResult)> callback,
                 bool result) {
                std::move(callback).Run(
                    result
                        ? CanCreateTranslatorResult::kReadily
                        : CanCreateTranslatorResult::kNoNotSupportedLanguage);
              },
              std::move(callbacks.first)),
          base::BindOnce(std::move(callbacks.second),
                         CanCreateTranslatorResult::kNoServiceCrashed)));
}

CanCreateTranslatorResult
OnDeviceTranslationServiceController::CanTranslateImpl(
    const std::string& source_lang,
    const std::string& target_lang) {
  std::set<LanguagePackKey> required_packs;
  std::vector<LanguagePackKey> required_not_installed_packs;
  std::vector<LanguagePackKey> to_be_registered_packs;
  CalculateLanguagePackRequirements(source_lang, target_lang, required_packs,
                                    required_not_installed_packs,
                                    to_be_registered_packs);
  if (required_packs.empty()) {
    // Empty `required_packs` means that the transltion for the specified
    // language pair is not supported.
    return CanCreateTranslatorResult::kNoNotSupportedLanguage;
  }

  if (!to_be_registered_packs.empty() &&
      kTranslationAPILimitLanguagePackCount.Get() &&
      to_be_registered_packs.size() +
              ComponentManager::GetRegisteredLanguagePacks().size() >
          kTranslationAPILimitLanguagePackCountMax) {
    // The number of installed language packs will exceed the limitation if the
    // new required language packs are installed.
    return CanCreateTranslatorResult::kNoExceedsLanguagePackCountLimitation;
  }

  if (required_not_installed_packs.empty()) {
    // All required language packages are installed.
    if (ComponentManager::GetTranslateKitLibraryPath().empty()) {
      // The TranslateKit library is not ready.
      return CanCreateTranslatorResult::kAfterDownloadLibraryNotReady;
    }
    // Both the TranslateKit library and the language packs are ready.
    return CanCreateTranslatorResult::kReadily;
  }

  if (ComponentManager::GetTranslateKitLibraryPath().empty()) {
    // Both the TranslateKit library and the language packs are not ready.
    return CanCreateTranslatorResult::
        kAfterDownloadLibraryAndLanguagePackNotReady;
  }
  // The required language packs are not ready.
  return CanCreateTranslatorResult::kAfterDownloadLanguagePackNotReady;
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
  if (ComponentManager::GetTranslateKitLibraryPath().empty()) {
    return;
  }
  const auto installed_packs = ComponentManager::GetInstalledLanguagePacks();
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

mojo::Remote<mojom::OnDeviceTranslationService>&
OnDeviceTranslationServiceController::GetRemote() {
  if (service_remote_) {
    return service_remote_;
  }

  auto receiver = service_remote_.BindNewPipeAndPassReceiver();
  service_remote_.reset_on_disconnect();

  const base::FilePath binary_path =
      ComponentManager::GetTranslateKitLibraryPath();
  CHECK(!binary_path.empty())
      << "Got an empty path to TranslateKit binary on the device.";

  std::vector<std::string> extra_switches;
  extra_switches.push_back(
      base::StrCat({kTranslateKitBinaryPath, "=", ToString(binary_path)}));

  content::ServiceProcessHost::Launch<mojom::OnDeviceTranslationService>(
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

  auto config = OnDeviceTranslationServiceConfig::New();
  std::vector<base::FilePath> package_pathes;
  ComponentManager::GetInstance().GetLanguagePackInfo(config->packages,
                                                      package_pathes);
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

// static
void OnDeviceTranslationServiceController::CalculateLanguagePackRequirements(
    const std::string& source_lang,
    const std::string& target_lang,
    std::set<LanguagePackKey>& required_packs,
    std::vector<LanguagePackKey>& required_not_installed_packs,
    std::vector<LanguagePackKey>& to_be_registered_packs) {
  CHECK(required_packs.empty());
  CHECK(required_not_installed_packs.empty());
  CHECK(to_be_registered_packs.empty());
  required_packs = CalculateRequiredLanguagePacks(source_lang, target_lang);
  const auto installed_packs = ComponentManager::GetInstalledLanguagePacks();
  base::ranges::set_difference(
      required_packs, installed_packs,
      std::back_inserter(required_not_installed_packs));
  const auto registered_packs = ComponentManager::GetRegisteredLanguagePacks();
  base::ranges::set_difference(required_not_installed_packs, registered_packs,
                               std::back_inserter(to_be_registered_packs));
}

// static
OnDeviceTranslationServiceController*
OnDeviceTranslationServiceController::GetInstance() {
  static base::NoDestructor<OnDeviceTranslationServiceController> instance;
  return instance.get();
}

}  // namespace on_device_translation
