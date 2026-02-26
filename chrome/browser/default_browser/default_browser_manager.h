// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_MANAGER_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_MANAGER_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "build/buildflag.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "url/gurl.h"

class BrowserProcess;
class Profile;

namespace default_browser {

class DefaultBrowserMonitor;
class DefaultBrowserNotificationObserver;

using DefaultBrowserCheckCompletionCallback =
    base::OnceCallback<void(DefaultBrowserState)>;
using DefaultBrowserChangedCallback =
    base::RepeatingCallback<void(DefaultBrowserState)>;
using ProfileProviderCallback = base::RepeatingCallback<Profile*()>;

// DefaultBrowserManager is the long-lived central coordinator and the public
// API for the default browser framework. It is responsible for selecting the
// correct setter and creating a controller, and provide general APIs for
// default-browser utilities.
class DefaultBrowserManager {
 public:
  DECLARE_USER_DATA(DefaultBrowserManager);

  // The unique ID used to identify and manage this system notification.
  static constexpr char kNotificationId[] = "default_browser_changed";

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
        const GURL& scheme,
        base::OnceCallback<void(const std::u16string&)> callback) = 0;
#endif  // BUILDFLAG(IS_WIN)
  };

  explicit DefaultBrowserManager(
      BrowserProcess* browser_process,
      std::unique_ptr<ShellDelegate> shell_delegate,
      ProfileProviderCallback profile_provider_callback);
  ~DefaultBrowserManager();

  DefaultBrowserManager(const DefaultBrowserManager&) = delete;
  DefaultBrowserManager& operator=(const DefaultBrowserManager&) = delete;

  static DefaultBrowserManager* From(BrowserProcess* browser_process);
  static std::unique_ptr<ShellDelegate> CreateDefaultDelegate();

  // Selects an appropriate setter, and create and returns a unique pointer to a
  // controller instance.
  static std::unique_ptr<DefaultBrowserController> CreateControllerFor(
      DefaultBrowserEntrypointType ui_entrypoint);

  Profile& GetProfile();

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
      DefaultBrowserChangedCallback callback);

 private:
  void OnDefaultBrowserCheckResult(
      default_browser::DefaultBrowserCheckCompletionCallback callback,
      default_browser::DefaultBrowserState default_state);

  // Performs additional validations on the default browser check's result to
  // detect potentially incorrect results.
  void PerformDefaultBrowserCheckValidations(
      default_browser::DefaultBrowserState default_state);

  // Called by the DefaultBrowserMonitor when a system-level change is detected.
  // Triggers a re-verification to get the latest browser default state.
  void OnMonitorDetectedChange();

  // Callback for when the async state check is completed.
  void NotifyObservers(DefaultBrowserState state);

  // Delegate for handling shell operations, such as checking and setting
  // default browser.
  const std::unique_ptr<ShellDelegate> shell_delegate_;

  // The platform default browser change monitor that handles the low-level
  // logic for detecting when the system's default browser has changed.
  std::unique_ptr<DefaultBrowserMonitor> monitor_;

  // The handler responsible for showing system notifications.
  std::unique_ptr<DefaultBrowserNotificationObserver> notification_observer_;

  // List of high-level observers (Notification, UI handlers, etc.)
  base::RepeatingCallbackList<void(DefaultBrowserState)> observers_;

  // The subscription to signals from the low-level `monitor_`.
  base::CallbackListSubscription monitor_subscription_;

  ProfileProviderCallback profile_provider_callback_;

  ui::ScopedUnownedUserData<DefaultBrowserManager> scoped_unowned_user_data_;
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_MANAGER_H_
