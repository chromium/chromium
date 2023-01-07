// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSIONS_BROWSER_INTERFACE_BINDERS_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSIONS_BROWSER_INTERFACE_BINDERS_H_

#include "mojo/public/cpp/bindings/binder_map.h"

namespace content {
class RenderFrameHost;
}

namespace extensions {

class Extension;

void PopulateChromeFrameBindersForExtension(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map,
    content::RenderFrameHost* render_frame_host,
    const Extension* extension);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSIONS_BROWSER_INTERFACE_BINDERS_H_
