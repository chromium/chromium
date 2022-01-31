// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_COLOR_INTERNALS_COLOR_INTERNALS_UI_H_
#define ASH_WEBUI_COLOR_INTERNALS_COLOR_INTERNALS_UI_H_

#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

// WebUIController for chrome://color-internals/.
class ColorInternalsUI : public ui::MojoWebUIController {
 public:
  explicit ColorInternalsUI(content::WebUI* web_ui);
  ColorInternalsUI(const ColorInternalsUI&) = delete;
  ColorInternalsUI& operator=(const ColorInternalsUI&) = delete;
  ~ColorInternalsUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_COLOR_INTERNALS_COLOR_INTERNALS_UI_H_
