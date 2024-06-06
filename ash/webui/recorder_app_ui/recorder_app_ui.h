// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_RECORDER_APP_UI_RECORDER_APP_UI_H_
#define ASH_WEBUI_RECORDER_APP_UI_RECORDER_APP_UI_H_

#include "ash/webui/recorder_app_ui/mojom/recorder_app.mojom.h"
#include "ash/webui/recorder_app_ui/url_constants.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ash {

class RecorderAppUI;

// WebUIConfig for chrome://recorder-app
class RecorderAppUIConfig : public content::WebUIConfig {
 public:
  RecorderAppUIConfig();
  ~RecorderAppUIConfig() override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome://recorder_app
class RecorderAppUI : public ui::MojoWebUIController,
                      public recorder_app::mojom::PageHandler {
 public:
  explicit RecorderAppUI(content::WebUI* web_ui);
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
  using OnDeviceModelService = on_device_model::mojom::OnDeviceModelService;

  WEB_UI_CONTROLLER_TYPE_DECL();

  OnDeviceModelService& GetOnDeviceModelService();

  // recorder_app::mojom::PageHandler:
  void LoadModel(
      const base::Uuid& model_id,
      mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
      LoadModelCallback callback) override;

  mojo::ReceiverSet<recorder_app::mojom::PageHandler> page_receivers_;

  mojo::Remote<OnDeviceModelService> on_device_model_service_;

  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RecorderAppUI> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WEBUI_RECORDER_APP_UI_RECORDER_APP_UI_H_
