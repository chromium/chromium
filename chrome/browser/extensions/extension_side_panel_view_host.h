// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SIDE_PANEL_VIEW_HOST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SIDE_PANEL_VIEW_HOST_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_view_host.h"

class Browser;
class GURL;

namespace content {
class SiteInstance;
class WebContents;
}  // namespace content

namespace extensions {

class Extension;

// The ExtensionHost for an extension that backs its side panel view.
class ExtensionSidePanelViewHost : public ExtensionViewHost {
 public:
  ExtensionSidePanelViewHost(const Extension* extension,
                             content::SiteInstance* site_instance,
                             const GURL& url,
                             Browser* browser,
                             content::WebContents* web_contents);

  ExtensionSidePanelViewHost(const ExtensionSidePanelViewHost&) = delete;
  ExtensionSidePanelViewHost& operator=(const ExtensionSidePanelViewHost&) =
      delete;
  ~ExtensionSidePanelViewHost() override;

  // ExtensionViewHost:
  Browser* GetBrowser() override;
  // This override is needed because GetBrowser() is not logically const.
  WindowController* GetExtensionWindowController() const override;

 private:
  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SIDE_PANEL_VIEW_HOST_H_
