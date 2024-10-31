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
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom-forward.h"

namespace on_device_translation {

class FileOperationProxyImpl;
enum class LanguagePackKey;

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

  // Creates a translator class that implements
  // `mojom::Translator`, and bind it with the
  // `receiver`.
  void CreateTranslator(
      const std::string& source_lang,
      const std::string& target_lang,
      base::OnceCallback<void(mojo::PendingRemote<mojom::Translator>)>
          callback);

  // Checks if the translate service can do translation from `source_lang` to
  // `target_lang`.
  void CanTranslate(
      const std::string& source_lang,
      const std::string& target_lang,
      base::OnceCallback<void(blink::mojom::CanCreateTranslatorResult)>
          callback);

 private:
  friend base::NoDestructor<OnDeviceTranslationServiceController>;

  // The information of a pending task. This is used to keep the tasks that are
  // waiting for the language packs to be installed.
  class PendingTask {
   public:
    PendingTask(std::set<LanguagePackKey> required_packs,
                base::OnceClosure once_closure);
    ~PendingTask();
    PendingTask(const PendingTask&) = delete;
    PendingTask& operator=(const PendingTask&) = delete;

    PendingTask(PendingTask&&);
    PendingTask& operator=(PendingTask&&);

    std::set<LanguagePackKey> required_packs;
    base::OnceClosure once_closure;
  };

  OnDeviceTranslationServiceController();
  ~OnDeviceTranslationServiceController();

  // Checks if the translate service can do translation from `source_lang` to
  // `target_lang`.
  blink::mojom::CanCreateTranslatorResult CanTranslateImpl(
      const std::string& source_lang,
      const std::string& target_lang);

  // Send the CreateTranslator IPC call to the OnDeviceTranslationService.
  void CreateTranslatorImpl(
      const std::string& source_lang,
      const std::string& target_lang,
      base::OnceCallback<void(mojo::PendingRemote<mojom::Translator>)>
          callback);

  // Called when the TranslateKitBinaryPath pref is changed.
  void OnTranslateKitBinaryPathChanged(const std::string& pref_name);

  // Called when the language pack key pref is changed.
  void OnLanguagePackKeyPrefChanged(const std::string& pref_name);

  mojo::Remote<mojom::OnDeviceTranslationService>& GetRemote();

  void MaybeRunPendingTasks();

  static void CalculateLanguagePackRequirements(
      const std::string& source_lang,
      const std::string& target_lang,
      std::set<LanguagePackKey>& required_packs,
      std::vector<LanguagePackKey>& required_not_installed_packs,
      std::vector<LanguagePackKey>& to_be_registered_packs);

  // TODO(crbug.com/335374928): implement the error handling for the translation
  // service crash.
  mojo::Remote<mojom::OnDeviceTranslationService> service_remote_;
  // Used to listen for changes on the pref values of TranslateKit component and
  // language pack components.
  PrefChangeRegistrar pref_change_registrar_;
  // The file operation proxy to access the files on disk. This is deleted on
  // a background task runner.
  std::unique_ptr<FileOperationProxyImpl, base::OnTaskRunnerDeleter>
      file_operation_proxy_;
  // The pending tasks that are waiting for the language packs to be installed.
  std::vector<PendingTask> pending_tasks_;
};

}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_H_
