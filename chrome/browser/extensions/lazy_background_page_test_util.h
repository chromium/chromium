// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_LAZY_BACKGROUND_PAGE_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_LAZY_BACKGROUND_PAGE_TEST_UTIL_H_

#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/notification_types.h"

namespace content {
class BrowserContext;
}

// Helper class to wait for a lazy background page to load and close again.
class LazyBackgroundObserver {
 public:
  explicit LazyBackgroundObserver(content::BrowserContext* browser_context)
      : page_created_(extensions::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
                      content::NotificationService::AllSources()),
        host_helper_(browser_context) {}

  void Wait() {
    WaitUntilLoaded();
    WaitUntilClosed();
  }

  void WaitUntilLoaded() {
    page_created_.Wait();
  }
  void WaitUntilClosed() {
    // TODO(devlin): This isn't guaranteed to be the background page for the
    // extension. We can update this when ExtensionHostTestHelper supports
    // filtering by host type.
    host_helper_.WaitForExtensionHostDestroyed();
  }

 private:
  // TODO(devlin): Replace this with an ExtensionHostTestHelper method.
  content::WindowedNotificationObserver page_created_;

  extensions::ExtensionHostTestHelper host_helper_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_LAZY_BACKGROUND_PAGE_TEST_UTIL_H_
