// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ai_overlay_dialog/ai_overlay_dialog_controller_android.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "ui/actions/actions.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/base_window.h"
#include "url/gurl.h"

namespace ttc {

AiOverlayDialogControllerAndroid::AiOverlayDialogControllerAndroid(
    BrowserWindowInterface* browser)
    : AiOverlayDialogController(browser) {}

AiOverlayDialogControllerAndroid::~AiOverlayDialogControllerAndroid() = default;

void AiOverlayDialogControllerAndroid::ShowOverlay() {
  if (!android_shell_web_contents_) {
    content::WebContents::CreateParams create_params(browser()->GetProfile());
    create_params.initially_hidden =
        false;  // Prevent background media throttling
    create_params.site_instance = content::SiteInstance::CreateForURL(
        browser()->GetProfile(),
        GURL(chrome::kChromeUIAiOverlayDialogUntrustedURL));
    android_shell_web_contents_ =
        content::WebContents::Create(std::move(create_params));

    webui::SetBrowserWindowInterface(android_shell_web_contents_.get(),
                                     browser());
    android_shell_web_contents_->SetDelegate(this);
    if (browser()->GetWindow()) {
      if (auto* window = static_cast<ui::WindowAndroid*>(
              browser()->GetWindow()->GetNativeWindow())) {
        if (auto* view = android_shell_web_contents_->GetNativeView()) {
          window->AddChild(view);
        }
      }
    }

    // Only navigate on creation: an existing WebContents is already at this
    // URL, and reloading it would discard the overlay's state.
    content::NavigationController::LoadURLParams params{
        GURL(chrome::kChromeUIAiOverlayDialogUntrustedURL)};
    android_shell_web_contents_->GetController().LoadURLWithParams(params);
  }
}

void AiOverlayDialogControllerAndroid::HideOverlay() {
  if (android_shell_web_contents_) {
    if (auto* view = android_shell_web_contents_->GetNativeView()) {
      view->RemoveFromParent();
    }
    android_shell_web_contents_.reset();
  }
}

bool AiOverlayDialogControllerAndroid::IsOverlayShowing() const {
  return android_shell_web_contents_ != nullptr;
}

}  // namespace ttc
