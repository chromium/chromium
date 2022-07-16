// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_FIRMWARE_UPDATE_UI_FIRMWARE_UPDATE_APP_UI_H_
#define ASH_WEBUI_FIRMWARE_UPDATE_UI_FIRMWARE_UPDATE_APP_UI_H_

#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {

class FirmwareUpdateAppUI : public ui::MojoWebUIController {
 public:
  explicit FirmwareUpdateAppUI(content::WebUI* web_ui);
  FirmwareUpdateAppUI(const FirmwareUpdateAppUI&) = delete;
  FirmwareUpdateAppUI& operator=(const FirmwareUpdateAppUI&) = delete;
  ~FirmwareUpdateAppUI() override;
};

}  // namespace ash

#endif  // ASH_WEBUI_FIRMWARE_UPDATE_UI_FIRMWARE_UPDATE_APP_UI_H_