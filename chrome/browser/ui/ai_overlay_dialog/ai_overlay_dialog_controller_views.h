// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_CONTROLLER_VIEWS_H_

#include "chrome/browser/ui/ai_overlay_dialog/ai_overlay_dialog_controller.h"

namespace views {
class WebView;
}  // namespace views

namespace ttc {

class AiOverlayDialogControllerViews : public AiOverlayDialogController {
 public:
  explicit AiOverlayDialogControllerViews(BrowserWindowInterface* browser);
  AiOverlayDialogControllerViews(const AiOverlayDialogControllerViews&) =
      delete;
  AiOverlayDialogControllerViews& operator=(
      const AiOverlayDialogControllerViews&) = delete;
  ~AiOverlayDialogControllerViews() override;

  void ShowOverlay() override;
  void HideOverlay() override;
  bool IsOverlayShowing() const override;

  // content::WebContentsDelegate:
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;

 private:
  views::WebView* GetActiveOverlayWebView() const;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_UI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_CONTROLLER_VIEWS_H_
