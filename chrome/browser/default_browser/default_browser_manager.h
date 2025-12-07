// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_MANAGER_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_MANAGER_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/default_browser/default_browser_controller.h"

namespace default_browser {

class DefaultBrowserMonitor;

using DefaultBrowserCheckCompletionCallback =
    base::OnceCallback<void(DefaultBrowserState)>;

// DefaultBrowserManager is the long-lived central coordinator and the public
// API for the default browser framework. It is responsible for selecting the
// correct setter and creating a controller, and provide general APIs for
// default-browser utilities.
class DefaultBrowserManager {
 public:
  // Delegate for performing shell-dependent operations.
  class ShellDelegate {
   public:
    virtual ~ShellDelegate() = 0;

    // Asynchronously checks whether the browser is the default.
    virtual void StartCheckIsDefault(
        shell_integration::DefaultWebClientWorkerCallback callback) = 0;

#if BUILDFLAG(IS_WIN)
    // Asynchronously fetches the program ID of the default client for the
    // given `scheme`.
    virtual void StartCheckDefaultClientProgId(
        const std::string& scheme,
        base::OnceCallback<void(const std::u16string&)> callback) = 0;
#endif  // BUILDFLAG(IS_WIN)
  };

  explicit DefaultBrowserManager(std::unique_ptr<ShellDelegate> shell_delegate);
  ~DefaultBrowserManager();

  DefaultBrowserManager(const DefaultBrowserManager&) = delete;
  DefaultBrowserManager& operator=(const DefaultBrowserManager&) = delete;

  static std::unique_ptr<ShellDelegate> CreateDefaultDelegate();

  // Selects an appropriate setter, and create and returns a unique pointer to a
  // controller instance.
  static std::unique_ptr<DefaultBrowserController> CreateControllerFor(
      DefaultBrowserEntrypointType ui_entrypoint);

  // Utility method to check the current default browser state asynchronously.
  void GetDefaultBrowserState(DefaultBrowserCheckCompletionCallback callback);

  // Registers a callback that will be invoked on the manager thread whenever
  // the system's default browser for HTTP/HTTPS protocols changes. The returned
  // subscription object MUST be kept in scope for as long as the caller wishes
  // to receive notifications. Destroying the subscription object will
  // unregister the callback.
  //
  // For now, only Windows platform will notify when a change occur.
  base::CallbackListSubscription RegisterDefaultBrowserChanged(
      base::RepeatingClosure callback);

 private:
  void OnDefaultBrowserCheckResult(
      default_browser::DefaultBrowserCheckCompletionCallback callback,
      default_browser::DefaultBrowserState default_state);

  // Delegate for handling shell operations, such as checking and setting
  // default browser.
  const std::unique_ptr<ShellDelegate> shell_delegate_;

  // The platform default browser change monitor that handles the low-level
  // logic for detecting when the system's default browser has changed.
  std::unique_ptr<DefaultBrowserMonitor> monitor_;
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_MANAGER_H_
