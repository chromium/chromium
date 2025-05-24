// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_INTERFACE_BINDERS_H_
#define CHROME_BROWSER_CHROME_BROWSER_INTERFACE_BINDERS_H_

#include "mojo/public/cpp/bindings/binder_map.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace chrome {
namespace internal {

// PopulateChromeFrameBinders() registers BrowserInterfaceBroker's
// GetInterface() handler callbacks for chrome-specific document-scoped
// interfaces.
void PopulateChromeFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map,
    content::RenderFrameHost* render_frame_host);

}  // namespace internal
}  // namespace chrome

#endif  // CHROME_BROWSER_CHROME_BROWSER_INTERFACE_BINDERS_H_
