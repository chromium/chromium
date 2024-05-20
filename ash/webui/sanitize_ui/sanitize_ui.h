// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SANITIZE_UI_SANITIZE_UI_H_
#define ASH_WEBUI_SANITIZE_UI_SANITIZE_UI_H_

#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/sanitize_ui/url_constants.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace ash {
class SanitizeDialogUI;

// The WebDialogUIConfig for chrome://sanitize
class SanitizeDialogUIConfig : public ChromeOSWebUIConfig<SanitizeDialogUI> {
 public:
  explicit SanitizeDialogUIConfig(
      CreateWebUIControllerFunc create_controller_func)
      : ChromeOSWebUIConfig(content::kChromeUIScheme,
                            ash::kChromeUISanitizeAppHost,
                            create_controller_func) {}
};

// The WebDialogUI for chrome://sanitize
class SanitizeDialogUI : public ui::MojoWebDialogUI {
 public:
  explicit SanitizeDialogUI(content::WebUI* web_ui);
  ~SanitizeDialogUI() override;

  SanitizeDialogUI(const SanitizeDialogUI&) = delete;
  SanitizeDialogUI& operator=(const SanitizeDialogUI&) = delete;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_SANITIZE_UI_SANITIZE_UI_H_
