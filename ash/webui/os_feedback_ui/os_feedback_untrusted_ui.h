// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_OS_FEEDBACK_UI_OS_FEEDBACK_UNTRUSTED_UI_H_
#define ASH_WEBUI_OS_FEEDBACK_UI_OS_FEEDBACK_UNTRUSTED_UI_H_

#include <memory>

#include "content/public/browser/webui_config.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace content {
class WebUI;
}  // namespace content

namespace ui {
class ColorChangeHandler;
}  // namespace ui

namespace ash {
namespace feedback {

class OsFeedbackUntrustedUI;

// Class that stores properties for the chrome-untrusted://os-feedback WebUI.
class OsFeedbackUntrustedUIConfig
    : public content::DefaultWebUIConfig<OsFeedbackUntrustedUI> {
 public:
  OsFeedbackUntrustedUIConfig();
  ~OsFeedbackUntrustedUIConfig() override;
};

// WebUI for chrome-untrusted://os-feedback, intended to be used by the file
// manager when untrusted content needs to be processed.
class OsFeedbackUntrustedUI : public ui::UntrustedWebUIController {
 public:
  explicit OsFeedbackUntrustedUI(content::WebUI* web_ui);
  OsFeedbackUntrustedUI(const OsFeedbackUntrustedUI&) = delete;
  OsFeedbackUntrustedUI& operator=(const OsFeedbackUntrustedUI&) = delete;
  ~OsFeedbackUntrustedUI() override;

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

 private:
  // The color change handler notifies the WebUI when the color provider
  // changes.
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace feedback
}  // namespace ash

#endif  // ASH_WEBUI_OS_FEEDBACK_UI_OS_FEEDBACK_UNTRUSTED_UI_H_
