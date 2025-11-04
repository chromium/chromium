// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_MANAGER_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_MANAGER_H_

namespace default_browser {

// DefaultBrowserManager is the long-lived central coordinator and the public
// API for the default browser framework. It is responsible for selecting the
// correct setter and create a controller, and provide general APIs for
// default-browser utilities.
class DefaultBrowserManager {
 public:
  DefaultBrowserManager() = default;
  ~DefaultBrowserManager() = default;

  DefaultBrowserManager(const DefaultBrowserManager&) = delete;
  DefaultBrowserManager& operator=(const DefaultBrowserManager&) = delete;
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_MANAGER_H_
