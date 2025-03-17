// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_PLATFORM_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_PLATFORM_DELEGATE_H_

#include "base/memory/raw_ref.h"
#include "build/build_config.h"

class GURL;

namespace extensions {

#if BUILDFLAG(IS_ANDROID)
class ExtensionPlatformBrowserTest;
using ExtensionBrowserTestParent = ExtensionPlatformBrowserTest;
#else
class ExtensionBrowserTest;
using ExtensionBrowserTestParent = ExtensionBrowserTest;
#endif

// A delegate to handle platform-specific logic needed by ExtensionBrowserTest
// (and friends).
// TODO(devlin): Continue moving more code into this class to avoid if-defs
// proliferating the test harnesses themselves. This provides a cleaner
// split.
class ExtensionBrowserTestPlatformDelegate {
 public:
  explicit ExtensionBrowserTestPlatformDelegate(
      ExtensionBrowserTestParent& parent);
  ExtensionBrowserTestPlatformDelegate(
      const ExtensionBrowserTestPlatformDelegate&) = delete;
  ExtensionBrowserTestPlatformDelegate& operator=(
      const ExtensionBrowserTestPlatformDelegate&) = delete;
  ~ExtensionBrowserTestPlatformDelegate() = default;

  // Opens a URL. If `open_in_incognito` is true, this will open in an
  // incognito context; otherwise, opens in the primary browser window for
  // the test.
  void OpenURL(const GURL& url, bool open_in_incognito);

 private:
  // The parent test. Owns `this`.
  raw_ref<ExtensionBrowserTestParent> parent_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_PLATFORM_DELEGATE_H_
