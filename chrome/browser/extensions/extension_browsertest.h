// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_

#include "chrome/browser/extensions/extension_platform_browsertest.h"

namespace extensions {

// DO NOT ADD MORE CODE. This will go away soon.
// See https://crbug.com/404581990.
class ExtensionBrowserTest : public ExtensionPlatformBrowserTest {
 protected:
  explicit ExtensionBrowserTest(ContextType context_type = ContextType::kNone);
  ~ExtensionBrowserTest() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_
