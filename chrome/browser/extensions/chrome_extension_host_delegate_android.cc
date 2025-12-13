// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_host_delegate.h"

#include <memory>

#include "base/notimplemented.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents.h"

static_assert(BUILDFLAG(IS_ANDROID));

namespace extensions {

void ChromeExtensionHostDelegate::CreateTab(
    std::unique_ptr<content::WebContents> web_contents,
    const ExtensionId& extension_id,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture) {
  // TODO(crbug.com/373434594): Support opening tabs.
  NOTIMPLEMENTED();
}

}  // namespace extensions
