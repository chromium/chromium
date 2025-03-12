// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PLATFORM_APITEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PLATFORM_APITEST_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/desktop_android/desktop_android_extension_apitest.h"
#else
#include "chrome/browser/extensions/extension_apitest.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace extensions {

#if BUILDFLAG(IS_ANDROID)
using ExtensionPlatformApiTest = DesktopAndroidExtensionApiTest;
#else
using ExtensionPlatformApiTest = ExtensionApiTest;
#endif

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PLATFORM_APITEST_H_
