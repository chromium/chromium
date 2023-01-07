// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_INTERFACE_BINDERS_H_
#define CHROME_BROWSER_CHROME_BROWSER_INTERFACE_BINDERS_H_

#include "mojo/public/cpp/bindings/binder_map.h"

namespace content {
class RenderFrameHost;
class WebUIBrowserInterfaceBrokerRegistry;
}  // namespace content

namespace chrome {
namespace internal {

// The mechanism implemented by the PopulateChrome*FrameBinders() functions
// below will replace interface registries and binders used for handling
// InterfaceProvider's GetInterface() calls (see crbug.com/718652).

// PopulateChromeFrameBinders() registers BrowserInterfaceBroker's
// GetInterface() handler callbacks for chrome-specific document-scoped
// interfaces.
void PopulateChromeFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map,
    content::RenderFrameHost* render_frame_host);

// PopulateChromeWebUIFrameBinders() registers BrowserInterfaceBroker's
// GetInterface() handler callbacks for chrome-specific document-scoped
// interfaces used from WebUI pages (e.g. chrome://bluetooth-internals).
void PopulateChromeWebUIFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map,
    content::RenderFrameHost* render_frame_host);

// PopulateChromeWebUIFrameInterfaceBrokers registers BrowserInterfaceBrokers
// for each WebUI, these brokers are used to handle that WebUI's JavaScript
// Mojo.bindInterface calls.
void PopulateChromeWebUIFrameInterfaceBrokers(
    content::WebUIBrowserInterfaceBrokerRegistry& registry);

}  // namespace internal
}  // namespace chrome

#endif  // CHROME_BROWSER_CHROME_BROWSER_INTERFACE_BINDERS_H_
