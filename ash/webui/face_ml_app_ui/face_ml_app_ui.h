// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_FACE_ML_APP_UI_FACE_ML_APP_UI_H_
#define ASH_WEBUI_FACE_ML_APP_UI_FACE_ML_APP_UI_H_

#include "ash/webui/face_ml_app_ui/face_ml_page_handler.h"
#include "ash/webui/face_ml_app_ui/mojom/face_ml_app_ui.mojom.h"
#include "ash/webui/face_ml_app_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

class FaceMLPageHandler;

// The Web UI for chrome://face-ml.
class FaceMLAppUI : public ui::MojoWebUIController,
                    public mojom::face_ml_app::PageHandlerFactory {
 public:
  explicit FaceMLAppUI(content::WebUI* web_ui);
  FaceMLAppUI(const FaceMLAppUI&) = delete;
  FaceMLAppUI& operator=(const FaceMLAppUI&) = delete;
  ~FaceMLAppUI() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::face_ml_app::PageHandlerFactory> factory);

 private:
  // mojom::face_ml_app::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<mojom::face_ml_app::PageHandler> handler,
      mojo::PendingRemote<mojom::face_ml_app::Page> page) override;

  mojo::Receiver<mojom::face_ml_app::PageHandlerFactory> face_ml_page_factory_{
      this};
  std::unique_ptr<FaceMLPageHandler> face_ml_page_handler_;

  // Called navigating to a WebUI page to create page handler.
  void WebUIPrimaryPageChanged(content::Page& page) override;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

// The WebUIConfig for chrome://face-ml/.
class FaceMLAppUIConfig : public SystemWebAppUIConfig<FaceMLAppUI> {
 public:
  FaceMLAppUIConfig()
      : SystemWebAppUIConfig(kChromeUIFaceMLAppHost,
                             SystemWebAppType::FACE_ML) {}
};

}  // namespace ash

#endif  // ASH_WEBUI_FACE_ML_APP_UI_FACE_ML_APP_UI_H_
