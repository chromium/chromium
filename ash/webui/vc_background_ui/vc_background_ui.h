// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_VC_BACKGROUND_UI_VC_BACKGROUND_UI_H_
#define ASH_WEBUI_VC_BACKGROUND_UI_VC_BACKGROUND_UI_H_

#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash::vc_background_ui {

class VcBackgroundUI;

class VcBackgroundUIConfig : public SystemWebAppUIConfig<VcBackgroundUI> {
 public:
  VcBackgroundUIConfig();

  VcBackgroundUIConfig(const VcBackgroundUIConfig&) = delete;
  VcBackgroundUIConfig& operator=(const VcBackgroundUIConfig&) = delete;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class VcBackgroundUI : public ui::MojoWebUIController {
 public:
  explicit VcBackgroundUI(content::WebUI* web_ui);

  VcBackgroundUI(const VcBackgroundUI&) = delete;
  VcBackgroundUI& operator=(const VcBackgroundUI&) = delete;

  ~VcBackgroundUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash::vc_background_ui

#endif  // ASH_WEBUI_VC_BACKGROUND_UI_VC_BACKGROUND_UI_H_
