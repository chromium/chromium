// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_util.h"

#include "chrome/browser/ash/policy/dlp/dlp_content_manager.h"
#include "chrome/browser/ui/ash/capture_mode/chrome_capture_mode_delegate.h"
#include "chrome/browser/ui/ash/window_pin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"

namespace extensions {
namespace tabs_util {

void SetLockedFullscreenState(Browser* browser, bool pinned) {
  aura::Window* window = browser->window()->GetNativeWindow();
  DCHECK(window);

  // As this gets triggered from extensions, we might encounter this case.
  if (IsWindowPinned(window) == pinned)
    return;

  if (pinned) {
    // Pins from extension are always trusted.
    PinWindow(window, /*trusted=*/true);
  } else {
    UnpinWindow(window);
  }

  // Update the set of available browser commands.
  browser->command_controller()->LockedFullscreenStateChanged();
}

bool IsScreenshotRestricted(content::WebContents* web_contents) {
  return policy::DlpContentManager::Get()->IsScreenshotApiRestricted(
      ScreenshotArea::CreateForWindow(web_contents->GetNativeView()));
}

}  // namespace tabs_util
}  // namespace extensions
