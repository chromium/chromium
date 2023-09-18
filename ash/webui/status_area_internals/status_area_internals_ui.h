// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_STATUS_AREA_INTERNALS_STATUS_AREA_INTERNALS_UI_H_
#define ASH_WEBUI_STATUS_AREA_INTERNALS_STATUS_AREA_INTERNALS_UI_H_

#include "ash/webui/status_area_internals/mojom/status_area_internals.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

// The UI controller for ChromeOS Status Area Internals page.
class StatusAreaInternalsUI : public ui::MojoWebUIController {
 public:
  explicit StatusAreaInternalsUI(content::WebUI* web_ui);
  StatusAreaInternalsUI(const StatusAreaInternalsUI&) = delete;
  StatusAreaInternalsUI& operator=(const StatusAreaInternalsUI&) = delete;
  ~StatusAreaInternalsUI() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::status_area_internals::PageHandler>
          receiver);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();

  std::unique_ptr<mojom::status_area_internals::PageHandler> page_handler_;
};

// UI config for the class above.
class StatusAreaInternalsUIConfig
    : public content::DefaultWebUIConfig<StatusAreaInternalsUI> {
 public:
  StatusAreaInternalsUIConfig();
};

}  // namespace ash

#endif  // ASH_WEBUI_STATUS_AREA_INTERNALS_STATUS_AREA_INTERNALS_UI_H_
