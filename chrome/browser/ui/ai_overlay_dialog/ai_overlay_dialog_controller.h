// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace views {
class WebView;
}  // namespace views

class AiOverlayDialogController : public content::WebContentsDelegate {
 public:
  DECLARE_USER_DATA(AiOverlayDialogController);

  static AiOverlayDialogController* From(BrowserWindowInterface* browser);

  explicit AiOverlayDialogController(BrowserWindowInterface* browser);
  AiOverlayDialogController(const AiOverlayDialogController&) = delete;
  AiOverlayDialogController& operator=(const AiOverlayDialogController&) =
      delete;
  ~AiOverlayDialogController() override;

  // Shows the transparent overlay above the browser window.
  void ShowOverlay();

  // Hides the overlay.
  void HideOverlay();

  // Toggles the overlay visibility.
  void ToggleOverlay();

  bool IsOverlayShowing() const;

 private:
  views::WebView* GetActiveOverlayWebView() const;

  raw_ptr<BrowserWindowInterface> browser_;

  ui::ScopedUnownedUserData<AiOverlayDialogController>
      scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_CONTROLLER_H_
