// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_GRADUATION_GRADUATION_UI_H_
#define ASH_WEBUI_GRADUATION_GRADUATION_UI_H_

#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/graduation/url_constants.h"
#include "base/memory/weak_ptr.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class BrowserContext;
class WebUI;
}  // namespace content

namespace ash::graduation {

class GraduationUI;

class GraduationUIConfig : public ChromeOSWebUIConfig<GraduationUI> {
 public:
  GraduationUIConfig()
      : ChromeOSWebUIConfig(content::kChromeUIScheme,
                            kChromeUIGraduationAppHost) {}
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class GraduationUI : public ui::MojoWebUIController {
 public:
  explicit GraduationUI(content::WebUI* web_ui);
  ~GraduationUI() override;
  GraduationUI(const GraduationUI&) = delete;
  GraduationUI& operator=(const GraduationUI&) = delete;

 private:
  base::WeakPtrFactory<GraduationUI> weak_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash::graduation

#endif  // ASH_WEBUI_GRADUATION_GRADUATION_UI_H_
