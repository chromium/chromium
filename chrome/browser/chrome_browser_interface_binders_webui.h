// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_INTERFACE_BINDERS_WEBUI_H_
#define CHROME_BROWSER_CHROME_BROWSER_INTERFACE_BINDERS_WEBUI_H_

#include "mojo/public/cpp/bindings/binder_map.h"

namespace content {
class RenderFrameHost;
class WebUIBrowserInterfaceBrokerRegistry;
}  // namespace content


namespace chrome::internal {

// PopulateChromeWebUIFrameBinders() registers BrowserInterfaceBroker's
// GetInterface() handler callbacks for chrome-specific document-scoped
// interfaces used from WebUI pages (e.g. chrome://bluetooth-internals).
void PopulateChromeWebUIFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map,
    content::RenderFrameHost* render_frame_host);

// PopulateChromeWebUIFrameInterfaceBrokers() registers BrowserInterfaceBrokers
// for each WebUI, these brokers are used to handle that WebUI's JavaScript
// Mojo.bindInterface calls.
void PopulateChromeWebUIFrameInterfaceBrokers(
    content::WebUIBrowserInterfaceBrokerRegistry& registry);

} // namespace chrome::internal


#endif  // CHROME_BROWSER_CHROME_BROWSER_INTERFACE_BINDERS_WEBUI_H_
