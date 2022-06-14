// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DEMO_MODE_APP_UI_DEMO_MODE_APP_UI_H_
#define ASH_WEBUI_DEMO_MODE_APP_UI_DEMO_MODE_APP_UI_H_

#include "ash/webui/demo_mode_app_ui/mojom/demo_mode_app_ui.mojom.h"
#include "base/files/file_path.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

class DemoModeAppUIConfig : public content::WebUIConfig {
 public:
  explicit DemoModeAppUIConfig(
      base::RepeatingCallback<base::FilePath()> component_path_producer);
  ~DemoModeAppUIConfig() override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui) override;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;

 private:
  // Callback that provides the demo app component path to the WebUI controller.
  // The path can't be passed directly into the DemoModeAppUIConfig constructor
  // because the config is created during startup, whereas the component isn't
  // loaded until the active demo session has started
  //
  // TODO(b/234174220): Consider creating a Delegate class that provides the
  // component path instead
  base::RepeatingCallback<base::FilePath()> component_path_producer_;
};

// The WebUI for chrome://demo-mode-app
class DemoModeAppUI : public ui::MojoWebUIController,
                      public mojom::demo_mode::PageHandlerFactory {
 public:
  explicit DemoModeAppUI(content::WebUI* web_ui,
                         base::FilePath component_base_path);
  ~DemoModeAppUI() override;

  DemoModeAppUI(const DemoModeAppUI&) = delete;
  DemoModeAppUI& operator=(const DemoModeAppUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<mojom::demo_mode::PageHandlerFactory> factory);

  // Visible for testing
  static void SourceDataFromComponent(
      const base::FilePath& component_path,
      const std::string& resource_path,
      content::WebUIDataSource::GotDataCallback callback);

 private:
  // mojom::DemoModePageHandlerFactory
  void CreatePageHandler(
      mojo::PendingReceiver<mojom::demo_mode::PageHandler> handler) override;

  mojo::Receiver<mojom::demo_mode::PageHandlerFactory> demo_mode_page_factory_{
      this};

  std::unique_ptr<mojom::demo_mode::PageHandler> demo_mode_page_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_DEMO_MODE_APP_UI_DEMO_MODE_APP_UI_H_
