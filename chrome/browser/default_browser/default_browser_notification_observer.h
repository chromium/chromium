// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_NOTIFICATION_OBSERVER_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_NOTIFICATION_OBSERVER_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/default_browser/default_browser_manager.h"

namespace default_browser {

// Listens to default browser state changes and shows a notification
// when Chrome is no longer the default browser.
class DefaultBrowserNotificationObserver {
 public:
  using RegisterCallback = base::OnceCallback<base::CallbackListSubscription(
      base::RepeatingCallback<void(DefaultBrowserState)>)>;

  // Signature to provide the initial async check
  using InitialStateCheckCallback =
      base::OnceCallback<void(base::OnceCallback<void(DefaultBrowserState)>)>;

  DefaultBrowserNotificationObserver(
      RegisterCallback register_callback,
      InitialStateCheckCallback initial_state_check_callback,
      DefaultBrowserManager& manager);

  DefaultBrowserNotificationObserver(
      const DefaultBrowserNotificationObserver&) = delete;
  DefaultBrowserNotificationObserver& operator=(
      const DefaultBrowserNotificationObserver&) = delete;

  ~DefaultBrowserNotificationObserver();

 private:
  // Callback invoked when the DefaultBrowserManager detects a change in the
  // system default browser state.
  void OnDefaultBrowserStateChanged(DefaultBrowserState state);

  // The observer is automatically unregistered when this object is destroyed.
  base::CallbackListSubscription default_browser_change_subscription_;

  // Track the last known state to detect transitions.
  // Initialize to UNKNOWN so we don't fire on the very first check
  // unless we've confirmed a transition from IS_DEFAULT.
  shell_integration::DefaultWebClientState last_state_ =
      shell_integration::UNKNOWN_DEFAULT;

  // The default browser manager that provides default browser state updates
  // and coordinates shell integration. It outlive this object.
  const raw_ref<DefaultBrowserManager> manager_;

  base::WeakPtrFactory<DefaultBrowserNotificationObserver> weak_ptr_factory_{
      this};
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_NOTIFICATION_OBSERVER_H_
