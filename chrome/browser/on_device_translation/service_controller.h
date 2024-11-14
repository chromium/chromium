// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_H_

#include <optional>
#include <set>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/services/on_device_translation/public/mojom/translator.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom-forward.h"
#include "url/origin.h"

namespace on_device_translation {

class FileOperationProxyImpl;
enum class LanguagePackKey;
class ServiceControllerManager;

// This class is the controller that launches the on-device translation service
// and delegates the functionalities. It is designed to be shared by multiple
// `TranslationManagerImpl` instances.  A single instance of this class is
// created for each pair of browser context and origin.
// TODO(crbug.com/364795294): This class does not support Android yet.
class OnDeviceTranslationServiceController
    : public base::RefCounted<OnDeviceTranslationServiceController> {
 public:
  OnDeviceTranslationServiceController(ServiceControllerManager* manager,
                                       const url::Origin& origin);

  OnDeviceTranslationServiceController(
      const OnDeviceTranslationServiceController&) = delete;
  OnDeviceTranslationServiceController& operator=(
      const OnDeviceTranslationServiceController&) = delete;

  // Creates a translator class that implements `mojom::Translator` for the
  // given language pair.
  void CreateTranslator(
      const std::string& source_lang,
      const std::string& target_lang,
      base::OnceCallback<
          void(base::expected<mojo::PendingRemote<mojom::Translator>,
                              blink::mojom::CreateTranslatorError>)> callback);

  // Checks if the translate service can do translation from `source_lang` to
  // `target_lang`.
  void CanTranslate(
      const std::string& source_lang,
      const std::string& target_lang,
      base::OnceCallback<void(blink::mojom::CanCreateTranslatorResult)>
          callback);

  // Sets the service idle timeout for testing. This must be called before the
  // service is started.
  void SetServiceIdleTimeoutForTesting(base::TimeDelta service_idle_timeout);

  // Returns true if the service is running.
  bool IsServiceRunning() const { return !!service_remote_; }

 private:
  friend base::RefCounted<OnDeviceTranslationServiceController>;

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
      base::OnceCallback<
          void(base::expected<mojo::PendingRemote<mojom::Translator>,
                              blink::mojom::CreateTranslatorError>)> callback);

  // Called when the TranslateKitBinaryPath pref is changed.
  void OnTranslateKitBinaryPathChanged(const std::string& pref_name);

  // Called when the language pack key pref is changed.
  void OnLanguagePackKeyPrefChanged(const std::string& pref_name);

  // Tries to start the service if it is not already running. Returns true if
  // the service is running or is started successfully.
  bool MaybeStartService();

  void MaybeRunPendingTasks();

  // Called when the service is idle and the idle timeout is reached.
  void OnServiceIdle();

  static void CalculateLanguagePackRequirements(
      const std::string& source_lang,
      const std::string& target_lang,
      std::set<LanguagePackKey>& required_packs,
      std::vector<LanguagePackKey>& required_not_installed_packs,
      std::vector<LanguagePackKey>& to_be_registered_packs);

  // The manager that manages the service controller. This `manager_` is owned
  // by the BrowserContext, and `this` is owned by the `TranslationManagerImpl`
  // instances which are DocumentUserData. So `manager_` must outlive `this`.
  const raw_ptr<ServiceControllerManager> manager_;

  // The origin of the web page that created this service controller.
  const url::Origin origin_;

  // The idle timeout for the translation service. When the service is idle for
  // this amount of time, the service will be terminated.
  base::TimeDelta service_idle_timeout_;
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
