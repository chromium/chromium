// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/side_panel_helper.h"

#include "base/check.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "extensions/browser/extension_web_contents_observer.h"

namespace extensions {

SidePanelHelper::SidePanelHelper(content::WebContents* web_contents)
    : content::WebContentsUserData<SidePanelHelper>(*web_contents) {
  ExtensionWebContentsObserver::GetForWebContents(web_contents)
      ->dispatcher()
      ->set_delegate(this);
}

WindowController* SidePanelHelper::GetExtensionWindowController() {
  BrowserWindowInterface* browser_window_interface =
      webui::GetBrowserWindowInterface(&GetWebContents());
  return browser_window_interface
             ? BrowserExtensionWindowController::From(browser_window_interface)
             : nullptr;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SidePanelHelper);

}  // namespace extensions
