// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_TEST_SUPPORT_SCOPED_FAKE_DEFAULT_BROWSER_MANAGER_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_TEST_SUPPORT_SCOPED_FAKE_DEFAULT_BROWSER_MANAGER_H_

#include <memory>

namespace default_browser {

class DefaultBrowserManager;

// Creates the `DefaultBrowserManager` for unit tests that don't set up global
// features. It uses a fake shell and monitor, and its setters report Chrome as
// default asynchronously, like real setters, so accepting a prompt never opens
// Windows Settings.
class ScopedFakeDefaultBrowserManager {
 public:
  ScopedFakeDefaultBrowserManager();
  ScopedFakeDefaultBrowserManager(const ScopedFakeDefaultBrowserManager&) =
      delete;
  ScopedFakeDefaultBrowserManager& operator=(
      const ScopedFakeDefaultBrowserManager&) = delete;
  ~ScopedFakeDefaultBrowserManager();

  DefaultBrowserManager& manager() { return *manager_; }

 private:
  std::unique_ptr<DefaultBrowserManager> manager_;
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_TEST_SUPPORT_SCOPED_FAKE_DEFAULT_BROWSER_MANAGER_H_
