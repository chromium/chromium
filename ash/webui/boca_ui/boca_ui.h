// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_UI_BOCA_UI_H_
#define ASH_WEBUI_BOCA_UI_BOCA_UI_H_

#include "ash/webui/boca_ui/url_constants.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace ash {

// The WebUI for chrome-untrusted://boca-app/. Boca app is directly served in
// main frame.
class BocaUI : public ui::UntrustedWebUIController {
 public:
  explicit BocaUI(content::WebUI* web_ui);
  BocaUI(const BocaUI&) = delete;
  BocaUI& operator=(const BocaUI&) = delete;
  ~BocaUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_BOCA_UI_BOCA_UI_H_
