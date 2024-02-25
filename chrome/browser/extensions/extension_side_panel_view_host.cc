// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_side_panel_view_host.h"

#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "url/gurl.h"

namespace extensions {

ExtensionSidePanelViewHost::ExtensionSidePanelViewHost(
    const Extension* extension,
    content::SiteInstance* site_instance,
    const GURL& url,
    Browser* browser,
    content::WebContents* web_contents)
    : ExtensionViewHost(extension,
                        site_instance,
                        url,
                        mojom::ViewType::kExtensionSidePanel,
                        browser),
      web_contents_(web_contents) {
  // Only one of `browser` or `web_contents` should be defined, depending on
  // whether this class is hosting an extension's global or tab-specific side
  // panel view.
  DCHECK(browser == nullptr ^ web_contents == nullptr);
}

ExtensionSidePanelViewHost::~ExtensionSidePanelViewHost() = default;

Browser* ExtensionSidePanelViewHost::GetBrowser() {
  // Returns the browser object if available, otherwise searches for the browser
  // that currently owns the tab-based `web_contents_`.
  if (Browser* browser = ExtensionViewHost::GetBrowser()) {
    return browser;
  }

  DCHECK(web_contents_);
  return chrome::FindBrowserWithTab(web_contents_);
}

WindowController* ExtensionSidePanelViewHost::GetExtensionWindowController()
    const {
  if (WindowController* window_controller =
          ExtensionViewHost::GetExtensionWindowController()) {
    return window_controller;
  }

  DCHECK(web_contents_);
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  return browser ? browser->extension_window_controller() : nullptr;
}

}  // namespace extensions
