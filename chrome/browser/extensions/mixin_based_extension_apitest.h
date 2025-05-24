// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_MIXIN_BASED_EXTENSION_APITEST_H_
#define CHROME_BROWSER_EXTENSIONS_MIXIN_BASED_EXTENSION_APITEST_H_

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"

namespace extensions {

// Base class for extension API test fixtures that are using mixins.
using MixinBasedExtensionApiTest =
    InProcessBrowserTestMixinHostSupport<ExtensionApiTest>;

}  // namespace extensions

extern template class InProcessBrowserTestMixinHostSupport<
    extensions::ExtensionApiTest>;

#endif  // CHROME_BROWSER_EXTENSIONS_MIXIN_BASED_EXTENSION_APITEST_H_
