// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_FACE_ML_APP_UI_FACE_ML_APP_UI_H_
#define ASH_WEBUI_FACE_ML_APP_UI_FACE_ML_APP_UI_H_

#include <memory>

#include "ash/webui/face_ml_app_ui/face_ml_page_handler.h"
#include "ash/webui/face_ml_app_ui/face_ml_user_provider.h"
#include "ash/webui/face_ml_app_ui/mojom/face_ml_app_ui.mojom.h"
#include "ash/webui/face_ml_app_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

class FaceMLAppUI;
class FaceMLPageHandler;

// The WebUIConfig for chrome://face-ml.
class FaceMLAppUIConfig : public SystemWebAppUIConfig<FaceMLAppUI> {
 public:
  explicit FaceMLAppUIConfig(
      SystemWebAppUIConfig::CreateWebUIControllerFunc create_controller_func)
      : SystemWebAppUIConfig(ash::kChromeUIFaceMLAppHost,
                             SystemWebAppType::FACE_ML,
                             create_controller_func) {}
};

// The WebUI for chrome://face-ml.
class FaceMLAppUI : public ui::MojoWebUIController,
                    public mojom::face_ml_app::PageHandlerFactory {
 public:
  FaceMLAppUI(content::WebUI* web_ui,
              std::unique_ptr<FaceMLUserProvider> user_provider);
  FaceMLAppUI(const FaceMLAppUI&) = delete;
  FaceMLAppUI& operator=(const FaceMLAppUI&) = delete;
  ~FaceMLAppUI() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::face_ml_app::PageHandlerFactory> factory);
  FaceMLUserProvider* GetUserProvider() { return user_provider_.get(); }

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
  std::unique_ptr<FaceMLUserProvider> user_provider_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_FACE_ML_APP_UI_FACE_ML_APP_UI_H_
