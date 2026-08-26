// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ai_overlay_dialog/ai_overlay_dialog_controller_android.h"

#include "base/notimplemented.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/actions/actions.h"
#include "url/gurl.h"

namespace ttc {

AiOverlayDialogControllerAndroid::AiOverlayDialogControllerAndroid(
    BrowserWindowInterface* browser)
    : AiOverlayDialogController(browser) {}

AiOverlayDialogControllerAndroid::~AiOverlayDialogControllerAndroid() = default;

void AiOverlayDialogControllerAndroid::ShowOverlay() {
  if (android_shell_web_contents_) {
    webui::SetBrowserWindowInterface(android_shell_web_contents_.get(),
                                     browser());
    android_shell_web_contents_->SetDelegate(this);
    content::NavigationController::LoadURLParams params{
        GURL(chrome::kChromeUIAiOverlayDialogUntrustedURL)};
    android_shell_web_contents_->GetController().LoadURLWithParams(params);
  }
}

void AiOverlayDialogControllerAndroid::HideOverlay() {
  NOTIMPLEMENTED();
}

bool AiOverlayDialogControllerAndroid::IsOverlayShowing() const {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace ttc
