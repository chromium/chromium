// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DEMO_MODE_APP_UI_DEMO_MODE_APP_UNTRUSTED_UI_H_
#define ASH_WEBUI_DEMO_MODE_APP_UI_DEMO_MODE_APP_UNTRUSTED_UI_H_

#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/demo_mode_app_ui/demo_mode_app_delegate.h"
#include "ash/webui/demo_mode_app_ui/mojom/demo_mode_app_untrusted_ui.mojom.h"
#include "base/files/file_path.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace ash {

class DemoModeAppUntrustedUI;

class DemoModeAppUntrustedUIConfig
    : public ChromeOSWebUIConfig<DemoModeAppUntrustedUI> {
 public:
  explicit DemoModeAppUntrustedUIConfig(
      CreateWebUIControllerFunc create_controller_func);
  ~DemoModeAppUntrustedUIConfig() override;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome-untrusted://demo-mode-app
class DemoModeAppUntrustedUI
    : public ui::UntrustedWebUIController,
      public mojom::demo_mode::UntrustedPageHandlerFactory {
 public:
  explicit DemoModeAppUntrustedUI(
      content::WebUI* web_ui,
      base::FilePath component_base_path,
      std::unique_ptr<DemoModeAppDelegate> delegate);
  ~DemoModeAppUntrustedUI() override;

  DemoModeAppUntrustedUI(const DemoModeAppUntrustedUI&) = delete;
  DemoModeAppUntrustedUI& operator=(const DemoModeAppUntrustedUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<mojom::demo_mode::UntrustedPageHandlerFactory>
          factory);

  // Visible for testing
  static void SourceDataFromComponent(
      const base::FilePath& component_path,
      const std::string& resource_path,
      content::WebUIDataSource::GotDataCallback callback);

  DemoModeAppDelegate& delegate() { return *delegate_; }

 private:
  // mojom::DemoModePageHandlerFactory
  void CreatePageHandler(
      mojo::PendingReceiver<mojom::demo_mode::UntrustedPageHandler> handler)
      override;

  std::unique_ptr<DemoModeAppDelegate> delegate_;

  mojo::Receiver<mojom::demo_mode::UntrustedPageHandlerFactory>
      demo_mode_page_factory_{this};

  std::unique_ptr<mojom::demo_mode::UntrustedPageHandler>
      demo_mode_page_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_DEMO_MODE_APP_UI_DEMO_MODE_APP_UNTRUSTED_UI_H_
