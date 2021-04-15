// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONTENT_SHORTCUT_CUSTOMIZATION_UI_SHORTCUT_CUSTOMIZATION_APP_UI_H_
#define ASH_CONTENT_SHORTCUT_CUSTOMIZATION_UI_SHORTCUT_CUSTOMIZATION_APP_UI_H_

#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {

class ShortcutCustomizationAppUI : public ui::MojoWebUIController {
 public:
  explicit ShortcutCustomizationAppUI(content::WebUI* web_ui);
  ShortcutCustomizationAppUI(const ShortcutCustomizationAppUI&) = delete;
  ShortcutCustomizationAppUI& operator=(const ShortcutCustomizationAppUI&) =
      delete;
  ~ShortcutCustomizationAppUI() override;
};

}  // namespace ash

#endif  // ASH_CONTENT_SHORTCUT_CUSTOMIZATION_UI_SHORTCUT_CUSTOMIZATION_APP_UI_H_