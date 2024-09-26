// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_GRADUATION_GRADUATION_UI_H_
#define ASH_WEBUI_GRADUATION_GRADUATION_UI_H_

#include <memory>

#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/graduation/mojom/graduation_ui.mojom.h"
#include "ash/webui/graduation/url_constants.h"
#include "base/memory/weak_ptr.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace content {
class BrowserContext;
class WebUI;
}  // namespace content

namespace ash::graduation {

class GraduationUI;
class GraduationUiHandler;

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

  void BindInterface(
      mojo::PendingReceiver<graduation_ui::mojom::GraduationUiHandler>
          receiver);

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

 private:
  // The color change handler notifies the WebUI when the color provider
  // changes.
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  std::unique_ptr<GraduationUiHandler> ui_handler_;

  base::WeakPtrFactory<GraduationUI> weak_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash::graduation

#endif  // ASH_WEBUI_GRADUATION_GRADUATION_UI_H_
