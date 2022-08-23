// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_FACE_ML_APP_UI_FACE_ML_APP_UI_H_
#define ASH_WEBUI_FACE_ML_APP_UI_FACE_ML_APP_UI_H_

#include "ash/webui/face_ml_app_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "content/public/browser/webui_config.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

class FaceMLAppUI : public ui::MojoWebUIController {
 public:
  explicit FaceMLAppUI(content::WebUI* web_ui);
  FaceMLAppUI(const FaceMLAppUI&) = delete;
  FaceMLAppUI& operator=(const FaceMLAppUI&) = delete;
  ~FaceMLAppUI() override;

 private:
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
