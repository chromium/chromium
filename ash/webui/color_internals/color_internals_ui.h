// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_COLOR_INTERNALS_COLOR_INTERNALS_UI_H_
#define ASH_WEBUI_COLOR_INTERNALS_COLOR_INTERNALS_UI_H_

#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ui {
class ColorChangeHandler;
}
namespace ash {

// WebUIController for chrome://color-internals/.
class ColorInternalsUI : public ui::MojoWebUIController {
 public:
  explicit ColorInternalsUI(content::WebUI* web_ui);
  ColorInternalsUI(const ColorInternalsUI&) = delete;
  ColorInternalsUI& operator=(const ColorInternalsUI&) = delete;
  ~ColorInternalsUI() override;

  // Instantiates the implementor of the mojom::PageHandler mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

 private:
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_COLOR_INTERNALS_COLOR_INTERNALS_UI_H_
