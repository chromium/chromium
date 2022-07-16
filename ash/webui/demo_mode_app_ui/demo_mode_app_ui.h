// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DEMO_MODE_APP_UI_DEMO_MODE_APP_UI_H_
#define ASH_WEBUI_DEMO_MODE_APP_UI_DEMO_MODE_APP_UI_H_

#include "ash/webui/demo_mode_app_ui/mojom/demo_mode_app_ui.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

// The WebUI for chrome://demo-mode-app
class DemoModeAppUI : public ui::MojoWebUIController,
                      public mojom::demo_mode::PageHandlerFactory {
 public:
  explicit DemoModeAppUI(content::WebUI* web_ui);
  ~DemoModeAppUI() override;

  DemoModeAppUI(const DemoModeAppUI&) = delete;
  DemoModeAppUI& operator=(const DemoModeAppUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<mojom::demo_mode::PageHandlerFactory> factory);

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
