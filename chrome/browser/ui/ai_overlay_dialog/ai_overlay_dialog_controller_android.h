// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_UI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_CONTROLLER_ANDROID_H_

#include "chrome/browser/ui/ai_overlay_dialog/ai_overlay_dialog_controller.h"

namespace ttc {

class AiOverlayDialogControllerAndroid : public AiOverlayDialogController {
 public:
  explicit AiOverlayDialogControllerAndroid(BrowserWindowInterface* browser);
  AiOverlayDialogControllerAndroid(const AiOverlayDialogControllerAndroid&) =
      delete;
  AiOverlayDialogControllerAndroid& operator=(
      const AiOverlayDialogControllerAndroid&) = delete;
  ~AiOverlayDialogControllerAndroid() override;

  // AiOverlayDialogController:
  void ShowOverlay() override;
  void HideOverlay() override;
  bool IsOverlayShowing() const override;

 private:
  std::unique_ptr<content::WebContents> android_shell_web_contents_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_UI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_CONTROLLER_ANDROID_H_
