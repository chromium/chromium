// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_LAZY_BACKGROUND_PAGE_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_LAZY_BACKGROUND_PAGE_TEST_UTIL_H_

#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/common/mojom/view_type.mojom.h"

namespace content {
class BrowserContext;
}

// Helper class to wait for a lazy background page to load and close again.
// TODO(devlin): Remove this whole class and just use ExtensionHostTestHelper.
class LazyBackgroundObserver {
 public:
  explicit LazyBackgroundObserver(content::BrowserContext* browser_context)
      : host_helper_(browser_context) {
    host_helper_.RestrictToType(
        extensions::mojom::ViewType::kExtensionBackgroundPage);
  }

  void Wait() {
    WaitUntilLoaded();
    WaitUntilClosed();
  }

  void WaitUntilLoaded() { host_helper_.WaitForDocumentElementAvailable(); }
  void WaitUntilClosed() { host_helper_.WaitForHostDestroyed(); }

 private:
  extensions::ExtensionHostTestHelper host_helper_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_LAZY_BACKGROUND_PAGE_TEST_UTIL_H_
