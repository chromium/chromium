// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_H_

#include <optional>
#include <set>
#include <vector>

#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/services/on_device_translation/public/mojom/translator.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace on_device_translation {
enum class LanguagePackKey;
}  // namespace on_device_translation

// This class is the controller that launches the on-device translation service
// and delegates the functionalities.
// TODO(crbug.com/364795294): This class does not support Android yet.
class OnDeviceTranslationServiceController {
 public:
  OnDeviceTranslationServiceController(
      const OnDeviceTranslationServiceController&) = delete;
  OnDeviceTranslationServiceController& operator=(
      const OnDeviceTranslationServiceController&) = delete;

  static OnDeviceTranslationServiceController* GetInstance();

  // If the TranslateKit binary path is passed via the command line, returns the
  // binary path. If the TranslateKit binary is installed as a component,
  // returns the directory path of the component. Otherwise, returns an empty
  // path.
  static base::FilePath GetTranslateKitComponentPath();

  // Creates a translator class that implements
  // `on_device_translation::mojom::Translator`, and bind it with the
  // `receiver`.
  void CreateTranslator(
      const std::string& source_lang,
      const std::string& target_lang,
      mojo::PendingReceiver<on_device_translation::mojom::Translator> receiver,
      base::OnceCallback<void(bool)> callback);

  // Checks if the translate service can do translation from `source_lang` to
  // `target_lang`.
  void CanTranslate(const std::string& source_lang,
                    const std::string& target_lang,
                    base::OnceCallback<void(bool)> callback);

 private:
  friend base::NoDestructor<OnDeviceTranslationServiceController>;

  class FileOperationProxyImpl;

  // The information of a language pack.
  struct LanguagePackInfo {
    std::string language1;
    std::string language2;
    base::FilePath package_path;
  };

  // The information of a pending task. This is used to keep the tasks that are
  // waiting for the language packs to be installed.
  class PendingTask {
   public:
    PendingTask(std::set<on_device_translation::LanguagePackKey> required_packs,
                base::OnceClosure once_closure);
    ~PendingTask();
    PendingTask(const PendingTask&) = delete;
    PendingTask& operator=(const PendingTask&) = delete;

    PendingTask(PendingTask&&);
    PendingTask& operator=(PendingTask&&);

    std::set<on_device_translation::LanguagePackKey> required_packs;
    base::OnceClosure once_closure;
  };

  OnDeviceTranslationServiceController();
  ~OnDeviceTranslationServiceController();

  // Send the CreateTranslator IPC call to the OnDeviceTranslationService.
  void CreateTranslatorImpl(
      const std::string& source_lang,
      const std::string& target_lang,
      mojo::PendingReceiver<on_device_translation::mojom::Translator> receiver,
      base::OnceCallback<void(bool)> callback);

  // Returns the language packs that are installed or set by the command line.
  std::vector<LanguagePackInfo> GetLanguagePackInfo();

  // Registers the installed language pack components.
  void RegisterInstalledLanguagePackComponent();

  // Registers the language pack component.
  void RegisterLanguagePackComponent(on_device_translation::LanguagePackKey);

  // Called when the TranslateKitBinaryPath pref is changed.
  void OnTranslateKitBinaryPathChanged(const std::string& pref_name);

  // Called when the language pack key pref is changed.
  void OnLanguagePackKeyPrefChanged(const std::string& pref_name);

  mojo::Remote<on_device_translation::mojom::OnDeviceTranslationService>&
  GetRemote();

  void MaybeRunPendingTasks();

  void CalculateLanguagePackRequirements(
      const std::string& source_lang,
      const std::string& target_lang,
      std::set<on_device_translation::LanguagePackKey>& required_packs,
      std::vector<on_device_translation::LanguagePackKey>&
          required_not_installed_packs,
      std::vector<on_device_translation::LanguagePackKey>&
          to_be_downloaded_packs);

  // Get a list of LanguagePackInfo from the command line flag
  // `--translate-kit-packages`.
  static std::optional<std::vector<LanguagePackInfo>>
  GetLanguagePackInfoFromCommandLine();

  // TODO(crbug.com/335374928): implement the error handling for the translation
  // service crash.
  mojo::Remote<on_device_translation::mojom::OnDeviceTranslationService>
      service_remote_;
  // Used to listen for changes on the pref values of TranslateKit component and
  // language pack components.
  PrefChangeRegistrar pref_change_registrar_;
  // The language packs that are registered.
  std::set<on_device_translation::LanguagePackKey> registered_language_packs_;
  // The LanguagePackInfo from the command line. This is nullopt if the command
  // line flag `--translate-kit-packages` is not set.
  const std::optional<std::vector<LanguagePackInfo>>
      language_packs_from_command_line_;
  // The file operation proxy to access the files on disk. This is deleted on
  // a background task runner.
  std::unique_ptr<FileOperationProxyImpl, base::OnTaskRunnerDeleter>
      file_operation_proxy_;
  // The pending tasks that are waiting for the language packs to be installed.
  std::vector<PendingTask> pending_tasks_;
};

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_H_
