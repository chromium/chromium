// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_HOST_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_HOST_FACTORY_H_

#include <memory>

#include "build/build_config.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class BrowserWindowInterface;
class GURL;

namespace tabs {
class TabInterface;
}

namespace extensions {

class Extension;
class ExtensionViewHost;

// A utility class to make ExtensionViewHosts for UI views that are backed
// by extensions.
class ExtensionViewHostFactory {
 public:
  ExtensionViewHostFactory(const ExtensionViewHostFactory&) = delete;
  ExtensionViewHostFactory& operator=(const ExtensionViewHostFactory&) = delete;

  // Creates a new ExtensionHost with its associated view, grouping it in the
  // appropriate SiteInstance (and therefore process) based on the URL and
  // profile.
  static std::unique_ptr<ExtensionViewHost> CreatePopupHost(
      const GURL& url,
      BrowserWindowInterface* browser);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Creates a new ExtensionHost with its associated view, grouping it in the
  // appropriate SiteInstance (and therefore process) based on the URL and
  // profile.
  static std::unique_ptr<ExtensionViewHost> CreateSidePanelHost(
      const Extension& extension,
      const GURL& url,
      BrowserWindowInterface* browser,
      tabs::TabInterface* tab_interface);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_HOST_FACTORY_H_
