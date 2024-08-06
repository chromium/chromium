// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_FOCUS_MODE_FOCUS_MODE_UI_H_
#define ASH_WEBUI_FOCUS_MODE_FOCUS_MODE_UI_H_

#include <memory>

#include "ash/webui/focus_mode/mojom/focus_mode.mojom.h"
#include "content/public/browser/webui_config.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

class FocusModeTrackProvider;

// The WebUI for chrome://focus-mode-media.
class FocusModeUI : public ui::MojoWebUIController {
 public:
  explicit FocusModeUI(content::WebUI* web_ui);
  ~FocusModeUI() override;

  void BindInterface(
      mojo::PendingReceiver<focus_mode::mojom::TrackProvider> receiver);

 private:
  std::unique_ptr<FocusModeTrackProvider> track_provider_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

// The WebUIConfig for chrome://focus-mode-media.
class FocusModeUIConfig : public content::DefaultWebUIConfig<FocusModeUI> {
 public:
  FocusModeUIConfig();

  // content::DefaultWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

}  // namespace ash

#endif  // ASH_WEBUI_FOCUS_MODE_FOCUS_MODE_UI_H_
