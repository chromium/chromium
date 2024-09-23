// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/chromevox_panel.h"

#include <memory>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/constants.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

const char kChromeVoxPanelRelativeUrl[] = "/chromevox/panel/panel.html";
const char kDisableSpokenFeedbackURLFragment[] = "close";
const char kFocusURLFragment[] = "focus";
const char kFullscreenURLFragment[] = "fullscreen";
const char kWidgetName[] = "ChromeVoxPanel";
const int kPanelHeight = 44;

}  // namespace

DEFINE_ELEMENT_IDENTIFIER_VALUE(kChromeVoxPanelElementId);

class ChromeVoxPanel::ChromeVoxPanelWebContentsObserver
    : public content::WebContentsObserver {
 public:
  ChromeVoxPanelWebContentsObserver(content::WebContents* web_contents,
                                    ChromeVoxPanel* panel)
      : content::WebContentsObserver(web_contents), panel_(panel) {}

  ChromeVoxPanelWebContentsObserver(const ChromeVoxPanelWebContentsObserver&) =
      delete;
  ChromeVoxPanelWebContentsObserver& operator=(
      const ChromeVoxPanelWebContentsObserver&) = delete;

  ~ChromeVoxPanelWebContentsObserver() override {}

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    // The ChromeVox panel uses the URL fragment to communicate state
    // to this panel host.
    std::string fragment = web_contents()->GetLastCommittedURL().ref();
    if (fragment == kDisableSpokenFeedbackURLFragment)
      AccessibilityManager::Get()->EnableSpokenFeedback(false);
    else if (fragment == kFullscreenURLFragment)
      panel_->EnterFullscreen();
    else if (fragment == kFocusURLFragment)
      panel_->Focus();
    else
      panel_->ExitFullscreen();
  }

 private:
  raw_ptr<ChromeVoxPanel> panel_;
};

ChromeVoxPanel::ChromeVoxPanel(content::BrowserContext* browser_context)
    : AccessibilityPanel(browser_context, GetUrlForContent(), kWidgetName) {
  web_contents_observer_ = std::make_unique<ChromeVoxPanelWebContentsObserver>(
      GetWebContents(), this);
  GetContentsView()->SetProperty(views::kElementIdentifierKey,
                                 kChromeVoxPanelElementId);
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
  auto state = fullscreen ? AccessibilityPanelState::FULLSCREEN
                          : AccessibilityPanelState::FULL_WIDTH;
  AccessibilityController::Get()->SetAccessibilityPanelBounds(bounds, state);
}

std::string ChromeVoxPanel::GetUrlForContent() {
  std::string url(EXTENSION_PREFIX);
  url += extension_misc::kChromeVoxExtensionId;
  url += kChromeVoxPanelRelativeUrl;

  return url;
}

}  // namespace ash
