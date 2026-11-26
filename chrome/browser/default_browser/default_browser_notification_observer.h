// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_NOTIFICATION_OBSERVER_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_NOTIFICATION_OBSERVER_H_

#include <optional>
#include <set>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/default_browser/default_browser_manager.h"
#include "chrome/browser/profiles/profile_observer.h"

class Profile;

namespace default_browser {

// Listens to default browser state changes and shows a notification
// when Chrome is no longer the default browser.
class DefaultBrowserNotificationObserver : public ProfileObserver {
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

  ~DefaultBrowserNotificationObserver() override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

 private:
  // Callback invoked when the DefaultBrowserManager detects a change in the
  // system default browser state.
  void OnDefaultBrowserStateChanged(DefaultBrowserState state);

  // Displays the default-browser-changed OS notification.
  void ShowNotification();

  // Checks whether the notification is still displayed and closes it if Chrome
  // has become the default browser again.
  void MaybeCloseNotification();

  // Closes the notification and records `TimeUntilDefaultWithoutClick` if it
  // was still displayed when Chrome became the default browser again.
  void CloseNotificationIfDisplayed(base::TimeDelta elapsed,
                                    std::set<std::string> displayed_ids,
                                    bool supports_synchronization);

  // The observer is automatically unregistered when this object is destroyed.
  base::CallbackListSubscription default_browser_change_subscription_;

  // Track the last known state to detect transitions.
  // Initialize to UNKNOWN so we don't fire on the very first check
  // unless we've confirmed a transition from IS_DEFAULT.
  shell_integration::DefaultWebClientState last_state_ =
      shell_integration::UNKNOWN_DEFAULT;

  // Timestamp of the most recently shown notification that has not yet been
  // evaluated for closing on return to `IS_DEFAULT`.
  std::optional<base::TimeTicks> notification_shown_time_;

  // The default browser manager that provides default browser state updates
  // and coordinates shell integration. It outlive this object.
  const raw_ref<DefaultBrowserManager> manager_;

  // Observes the profile on which the active notification was shown so state is
  // cleared before `KeyedService` shutdown if the profile is destroyed early.
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  base::WeakPtrFactory<DefaultBrowserNotificationObserver> weak_ptr_factory_{
      this};
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_NOTIFICATION_OBSERVER_H_
