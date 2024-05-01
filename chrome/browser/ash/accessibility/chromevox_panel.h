// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_CHROMEVOX_PANEL_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_CHROMEVOX_PANEL_H_

#include "chrome/browser/ash/accessibility/accessibility_panel.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {

DECLARE_ELEMENT_IDENTIFIER_VALUE(kChromeVoxPanelElementId);

// Displays spoken feedback UI controls for the ChromeVox component extension
class ChromeVoxPanel : public AccessibilityPanel {
 public:
  explicit ChromeVoxPanel(content::BrowserContext* browser_context);

  ChromeVoxPanel(const ChromeVoxPanel&) = delete;
  ChromeVoxPanel& operator=(const ChromeVoxPanel&) = delete;

  ~ChromeVoxPanel() override;

  class ChromeVoxPanelWebContentsObserver;

 private:
  // Methods indirectly invoked by the component extension.
  void EnterFullscreen();
  void ExitFullscreen();
  void Focus();

  // Sends a request to the ash window manager.
  void SetAccessibilityPanelFullscreen(bool fullscreen);

  std::string GetUrlForContent();

  std::unique_ptr<ChromeVoxPanelWebContentsObserver> web_contents_observer_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_CHROMEVOX_PANEL_H_
