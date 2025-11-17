// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_MANAGER_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_MANAGER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "chrome/browser/default_browser/default_browser_controller.h"

namespace default_browser {

using DefaultBrowserCheckCompletionCallback =
    base::OnceCallback<void(DefaultBrowserState)>;

// DefaultBrowserManager is the long-lived central coordinator and the public
// API for the default browser framework. It is responsible for selecting the
// correct setter and creating a controller, and provide general APIs for
// default-browser utilities.
class DefaultBrowserManager {
 public:
  DefaultBrowserManager() = default;
  ~DefaultBrowserManager() = default;

  DefaultBrowserManager(const DefaultBrowserManager&) = delete;
  DefaultBrowserManager& operator=(const DefaultBrowserManager&) = delete;

  // Selects an appropriate setter, and create and returns a unique pointer to a
  // controller instance.
  static std::unique_ptr<DefaultBrowserController> CreateControllerFor(
      DefaultBrowserEntrypointType ui_entrypoint);

  // Utility method to check the current default browser state asynchronously.
  static void GetDefaultBrowserState(
      DefaultBrowserCheckCompletionCallback callback);
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_MANAGER_H_
