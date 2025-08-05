// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSIONS_BROWSER_INTERFACE_BINDERS_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSIONS_BROWSER_INTERFACE_BINDERS_H_

#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace content {
class BrowserContext;
class RenderFrameHost;
struct ServiceWorkerVersionBaseInfo;
}

namespace mojo {
template <typename>
class BinderMapWithContext;
}  // namespace mojo

namespace extensions {

class Extension;

void PopulateChromeFrameBindersForExtension(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map,
    content::RenderFrameHost* render_frame_host,
    const Extension* extension);

void PopulateChromeServiceWorkerBindersForExtension(
    mojo::BinderMapWithContext<const content::ServiceWorkerVersionBaseInfo&>*
        binder_map,
    content::BrowserContext* browser_context,
    const Extension* extension);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSIONS_BROWSER_INTERFACE_BINDERS_H_
