// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_FACE_ML_APP_UI_FACE_ML_APP_UNTRUSTED_UI_H_
#define ASH_WEBUI_FACE_ML_APP_UI_FACE_ML_APP_UNTRUSTED_UI_H_

#include "ash/webui/face_ml_app_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "content/public/browser/webui_config.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

// The Web UI for chrome-untrusted://face-ml.
class FaceMLAppUntrustedUI : public ui::UntrustedWebUIController {
 public:
  explicit FaceMLAppUntrustedUI(content::WebUI* web_ui);
  FaceMLAppUntrustedUI(const FaceMLAppUntrustedUI&) = delete;
  FaceMLAppUntrustedUI& operator=(const FaceMLAppUntrustedUI&) = delete;
  ~FaceMLAppUntrustedUI() override;
};

class FaceMLAppUntrustedUIConfig
    : public SystemWebAppUntrustedUIConfig<FaceMLAppUntrustedUI> {
 public:
  FaceMLAppUntrustedUIConfig()
      : SystemWebAppUntrustedUIConfig(kChromeUIFaceMLAppHost,
                                      SystemWebAppType::FACE_ML) {}

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

}  // namespace ash

#endif  // ASH_WEBUI_FACE_ML_APP_UI_FACE_ML_APP_UNTRUSTED_UI_H_
