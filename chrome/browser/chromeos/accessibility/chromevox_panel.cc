// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/chromevox_panel.h"

#include "ash/public/cpp/accessibility_controller.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/common/constants.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/views/widget/widget.h"

namespace {

const char kChromeVoxPanelRelativeUrl[] = "/background/panel/panel.html";
const char kDisableSpokenFeedbackURLFragment[] = "close";
const char kFocusURLFragment[] = "focus";
const char kFullscreenURLFragment[] = "fullscreen";
const char kWidgetName[] = "ChromeVoxPanel";
const int kPanelHeight = 35;

}  // namespace

class ChromeVoxPanel::ChromeVoxPanelWebContentsObserver
    : public content::WebContentsObserver {
 public:
  ChromeVoxPanelWebContentsObserver(content::WebContents* web_contents,
                                    ChromeVoxPanel* panel)
      : content::WebContentsObserver(web_contents), panel_(panel) {}
  ~ChromeVoxPanelWebContentsObserver() override {}

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    // The ChromeVox panel uses the URL fragment to communicate state
    // to this panel host.
    std::string fragment = web_contents()->GetLastCommittedURL().ref();
    if (fragment == kDisableSpokenFeedbackURLFragment)
      chromeos::AccessibilityManager::Get()->EnableSpokenFeedback(false);
    else if (fragment == kFullscreenURLFragment)
      panel_->EnterFullscreen();
    else if (fragment == kFocusURLFragment)
      panel_->Focus();
    else
      panel_->ExitFullscreen();
  }

 private:
  ChromeVoxPanel* panel_;

  DISALLOW_COPY_AND_ASSIGN(ChromeVoxPanelWebContentsObserver);
};

ChromeVoxPanel::ChromeVoxPanel(content::BrowserContext* browser_context)
    : AccessibilityPanel(browser_context, GetUrlForContent(), kWidgetName) {
  web_contents_observer_.reset(
      new ChromeVoxPanelWebContentsObserver(GetWebContents(), this));

  SetAccessibilityPanelFullscreen(false);
}

ChromeVoxPanel::~ChromeVoxPanel() {}

void ChromeVoxPanel::EnterFullscreen() {
  Focus();
  SetAccessibilityPanelFullscreen(true);
}

void ChromeVoxPanel::ExitFullscreen() {
  GetWidget()->Deactivate();
  GetWidget()->widget_delegate()->SetCanActivate(false);
  SetAccessibilityPanelFullscreen(false);
}

void ChromeVoxPanel::Focus() {
  GetWidget()->widget_delegate()->SetCanActivate(true);
  GetWidget()->Activate();
  GetContentsView()->RequestFocus();
}

void ChromeVoxPanel::SetAccessibilityPanelFullscreen(bool fullscreen) {
  gfx::Rect bounds(0, 0, 0, kPanelHeight);
  auto state = fullscreen ? ash::AccessibilityPanelState::FULLSCREEN
                          : ash::AccessibilityPanelState::FULL_WIDTH;
  ash::AccessibilityController::Get()->SetAccessibilityPanelBounds(bounds,
                                                                   state);
}

std::string ChromeVoxPanel::GetUrlForContent() {
  std::string url(EXTENSION_PREFIX);
  url += extension_misc::kChromeVoxExtensionId;
  url += kChromeVoxPanelRelativeUrl;

  return url;
}
