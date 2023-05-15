// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_HELP_APP_UI_HELP_APP_UI_H_
#define ASH_WEBUI_HELP_APP_UI_HELP_APP_UI_H_

#include <memory>

#include "ash/webui/help_app_ui/help_app_ui.mojom.h"
#include "ash/webui/help_app_ui/help_app_ui_delegate.h"
#include "ash/webui/help_app_ui/search/search.mojom.h"
#include "ash/webui/help_app_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "chromeos/ash/components/local_search_service/public/mojom/index.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

class HelpAppPageHandler;
class HelpAppUI;

// The WebUIConfig for chrome://help-app.
class HelpAppUIConfig : public SystemWebAppUIConfig<HelpAppUI> {
 public:
  explicit HelpAppUIConfig(
      SystemWebAppUIConfig::CreateWebUIControllerFunc create_controller_func)
      : SystemWebAppUIConfig(ash::kChromeUIHelpAppHost,
                             SystemWebAppType::HELP,
                             create_controller_func) {}
};

// The WebUI controller for chrome://help-app.
class HelpAppUI : public ui::MojoWebUIController,
                  public help_app::mojom::PageHandlerFactory {
 public:
  HelpAppUI(content::WebUI* web_ui,
            std::unique_ptr<HelpAppUIDelegate> delegate);
  ~HelpAppUI() override;

  HelpAppUI(const HelpAppUI&) = delete;
  HelpAppUI& operator=(const HelpAppUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<help_app::mojom::PageHandlerFactory> receiver);

  void BindInterface(
      mojo::PendingReceiver<local_search_service::mojom::Index> index_receiver);

  // The search handler is used to update the search index for launcher search.
  void BindInterface(
      mojo::PendingReceiver<help_app::mojom::SearchHandler> receiver);

  HelpAppUIDelegate* delegate() { return delegate_.get(); }

  bool IsJavascriptErrorReportingEnabled() override;

 private:
  // help_app::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<help_app::mojom::PageHandler> receiver) override;

  std::unique_ptr<HelpAppPageHandler> page_handler_;
  mojo::Receiver<help_app::mojom::PageHandlerFactory> page_factory_receiver_{
      this};
  std::unique_ptr<HelpAppUIDelegate> delegate_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_HELP_APP_UI_HELP_APP_UI_H_
