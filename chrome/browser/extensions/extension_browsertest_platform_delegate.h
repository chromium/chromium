// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_PLATFORM_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_PLATFORM_DELEGATE_H_

#include "base/memory/raw_ref.h"
#include "build/build_config.h"

class GURL;
class Profile;

namespace base {
class FilePath;
}

namespace extensions {
class Extension;
class ExtensionBrowserTest;

// A delegate to handle platform-specific logic needed by ExtensionBrowserTest
// (and friends).
// TODO(devlin): Continue moving more code into this class to avoid if-defs
// proliferating the test harnesses themselves. This provides a cleaner
// split.
class ExtensionBrowserTestPlatformDelegate {
 public:
  explicit ExtensionBrowserTestPlatformDelegate(ExtensionBrowserTest& parent);
  ExtensionBrowserTestPlatformDelegate(
      const ExtensionBrowserTestPlatformDelegate&) = delete;
  ExtensionBrowserTestPlatformDelegate& operator=(
      const ExtensionBrowserTestPlatformDelegate&) = delete;
  ~ExtensionBrowserTestPlatformDelegate() = default;

  // Returns the Profile associated with the parent test.
  Profile* GetProfile();

  // Called from InProcessBrowserTest::SetUpOnMainThread() to allow for
  // platform-specific initialization.
  void SetUpOnMainThread();

  // Opens a URL. If `open_in_incognito` is true, this will open in an
  // incognito context; otherwise, opens in the primary browser window for
  // the test.
  void OpenURL(const GURL& url, bool open_in_incognito);

  // Loads and launches the app from `path`, and returns it. Waits until the
  // launched app's WebContents has been created and finished loading. If the
  // app uses a guest view this will create two WebContents (one for the host
  // and one for the guest view). `uses_guest_view` is used to wait for the
  // second WebContents.
  // TODO(devlin): Move this to browsertest_util? It's not used _that_ much.
  const Extension* LoadAndLaunchApp(const base::FilePath& path,
                                    bool uses_guest_view);

  // Wait for the number of visible page actions to change to `count`.
  bool WaitForPageActionVisibilityChangeTo(int count);

 private:
  // The parent test. Owns `this`.
  raw_ref<ExtensionBrowserTest> parent_;

  // The default profile to be used.
  raw_ptr<Profile, AcrossTasksDanglingUntriaged> profile_ = nullptr;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_PLATFORM_DELEGATE_H_
