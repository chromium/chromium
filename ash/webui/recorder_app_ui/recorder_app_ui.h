// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_RECORDER_APP_UI_RECORDER_APP_UI_H_
#define ASH_WEBUI_RECORDER_APP_UI_RECORDER_APP_UI_H_

#include <vector>

#include "ash/webui/recorder_app_ui/mojom/recorder_app.mojom.h"
#include "ash/webui/recorder_app_ui/recorder_app_ui_delegate.h"
#include "ash/webui/recorder_app_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/soda.mojom.h"
#include "components/soda/soda_installer.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ash {

class RecorderAppUI;

// WebUIConfig for chrome://recorder-app
class RecorderAppUIConfig : public SystemWebAppUIConfig<RecorderAppUI> {
 public:
  explicit RecorderAppUIConfig(
      SystemWebAppUIConfig::CreateWebUIControllerFunc create_controller_func)
      : SystemWebAppUIConfig(kChromeUIRecorderAppHost,
                             SystemWebAppType::RECORDER,
                             create_controller_func) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome://recorder_app
class RecorderAppUI
    : public ui::MojoWebUIController,
      public recorder_app::mojom::PageHandler,
      public speech::SodaInstaller::Observer,
      public on_device_model::mojom::PlatformModelProgressObserver {
 public:
  using WithRealIdCallback =
      base::OnceCallback<void(const std::optional<std::string>&)>;
  using DeviceIdMappingCallback =
      base::RepeatingCallback<void(const std::string&, WithRealIdCallback)>;
  explicit RecorderAppUI(content::WebUI* web_ui,
                         std::unique_ptr<RecorderAppUIDelegate> delegate);
  ~RecorderAppUI() override;

  RecorderAppUI(const RecorderAppUI&) = delete;
  RecorderAppUI& operator=(const RecorderAppUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<recorder_app::mojom::PageHandler> receiver);
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

  static constexpr std::string GetWebUIName() { return "RecorderApp"; }

 private:
  using OnDeviceModelService =
      on_device_model::mojom::OnDeviceModelPlatformService;

  using MachineLearningService =
      chromeos::machine_learning::mojom::MachineLearningService;
  using SodaClientMojoRemote =
      mojo::PendingRemote<chromeos::machine_learning::mojom::SodaClient>;
  using SodaRecognizerMojoReceiver =
      mojo::PendingReceiver<chromeos::machine_learning::mojom::SodaRecognizer>;

  using ModelState = recorder_app::mojom::ModelState;

  WEB_UI_CONTROLLER_TYPE_DECL();

  void EnsureOnDeviceModelService();

  mojo::Remote<MachineLearningService>& GetMlService();

  void UpdateSodaState(ModelState state);

  void GetPlatformModelStateCallback(
      const base::Uuid& model_id,
      on_device_model::mojom::PlatformModelState state);

  void UpdateModelState(const base::Uuid& model_id, ModelState state);

  void GetMicrophoneInfoWithDeviceId(
      GetMicrophoneInfoCallback callback,
      const std::optional<std::string>& device_id);

  // recorder_app::mojom::PageHandler:
  void LoadModel(
      const base::Uuid& model_id,
      mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
      LoadModelCallback callback) override;

  void AddModelMonitor(
      const base::Uuid& model_id,
      ::mojo::PendingRemote<recorder_app::mojom::ModelStateMonitor> monitor,
      AddModelMonitorCallback callback) override;

  void LoadSpeechRecognizer(SodaClientMojoRemote soda_client,
                            SodaRecognizerMojoReceiver soda_recognizer,
                            LoadSpeechRecognizerCallback callback) override;

  void InstallSoda(InstallSodaCallback callback) override;

  void AddSodaMonitor(
      ::mojo::PendingRemote<recorder_app::mojom::ModelStateMonitor> monitor,
      AddSodaMonitorCallback callback) override;

  void OpenAiFeedbackDialog(const std::string& description_template) override;

  void GetMicrophoneInfo(const std::string& source_id,
                         GetMicrophoneInfoCallback callback) override;

  // speech::SodaInstaller::Observer
  void OnSodaInstalled(speech::LanguageCode language_code) override;

  void OnSodaInstallError(speech::LanguageCode language_code,
                          speech::SodaInstaller::ErrorCode error_code) override;

  void OnSodaProgress(speech::LanguageCode language_code,
                      int progress) override;

  // on_device_model::mojom::PlatformModelProgressObserver:
  void Progress(double progress) override;

  mojo::Remote<MachineLearningService> ml_service_;

  std::unique_ptr<RecorderAppUIDelegate> delegate_;

  mojo::ReceiverSet<recorder_app::mojom::PageHandler> page_receivers_;

  mojo::RemoteSet<recorder_app::mojom::ModelStateMonitor> soda_monitors_;

  ModelState soda_state_;

  std::map<base::Uuid, mojo::RemoteSet<recorder_app::mojom::ModelStateMonitor>>
      model_monitors_;

  mojo::ReceiverSet<on_device_model::mojom::PlatformModelProgressObserver,
                    base::Uuid>
      model_progress_receivers_;

  base::flat_map<base::Uuid, ModelState> model_states_;

  mojo::Remote<OnDeviceModelService> on_device_model_service_;

  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  DeviceIdMappingCallback device_id_mapping_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RecorderAppUI> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WEBUI_RECORDER_APP_UI_RECORDER_APP_UI_H_
