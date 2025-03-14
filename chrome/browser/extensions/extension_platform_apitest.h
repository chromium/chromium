// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PLATFORM_APITEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PLATFORM_APITEST_H_

#include "chrome/browser/extensions/extension_apitest.h"

namespace extensions {

// TODO(https://crbug.com/401522580): Delete this file. We don't need it
// anymore, now that ExtensionApiTest is compiled for both desktop android and
// other platforms.
using ExtensionPlatformApiTest = ExtensionApiTest;

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PLATFORM_APITEST_H_
