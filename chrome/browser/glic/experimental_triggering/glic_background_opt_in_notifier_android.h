// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_BACKGROUND_OPT_IN_NOTIFIER_ANDROID_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_BACKGROUND_OPT_IN_NOTIFIER_ANDROID_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"

namespace glic {

// Process-wide owner of the single `GlicExperimentalOptInBridge` notification.
// Shows the notification when the first handler subscribes and hides it when
// the last subscription is dropped, across all `Profile`s and coordinators.
//
// Foreground readiness on Android can complete across three stages depending on
// how Chrome was launched:
// - Warm activity start
// (`JNI_GlicExperimentalOptInBridge_OnActivityStartedOrResumed`):
//   covers bringing an already-initialized `ChromeTabbedActivity` to the
//   foreground when the native browser window and active tab already exist.
// - Cold start or recreated activity (`OnBrowserCreated`, `OnBrowserActivated`,
//   `OnTabModelAdded`): covers post-native startup where the browser window and
//   `TabModel` are registered after `Activity.onResume()` has already fired.
// - Session restore or launch-intent tab creation (`DidSelectTab`, `DidAddTab`,
//   `OnDidActiveStateChange`): covers asynchronous tab restore or intent
//   handling populating the active tab and activating the regular `TabModel`.
class BackgroundOptInNotifier : public BrowserCollectionObserver,
                                public TabModelListObserver,
                                public TabModelObserver {
 public:
  enum class Event { kForegrounded, kCancelled };

  static BackgroundOptInNotifier& Get();

  BackgroundOptInNotifier(const BackgroundOptInNotifier&) = delete;
  BackgroundOptInNotifier& operator=(const BackgroundOptInNotifier&) = delete;

  // Subscribes `cb` and ensures the notification is shown. Returns an empty
  // subscription if showing the notification failed.
  base::CallbackListSubscription Subscribe(
      base::RepeatingCallback<void(Event)> cb);

  void Notify(Event event);

  // Coalesces foreground notifications onto the next UI task so `Activity`
  // lifecycle and cold-start initialization tasks finish setting up browser
  // windows and `TabModel`s before deferred opt-in checks run.
  void ScheduleForegroundNotification();

  void SetCallbacksForTesting(base::RepeatingCallback<bool()> show_cb,
                              base::RepeatingClosure hide_cb);

  // BrowserCollectionObserver:
  // Covers cold start or newly created `ChromeTabbedActivity` instances where
  // the native browser window is created or activated after `onResume()`.
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserActivated(BrowserWindowInterface* browser) override;

  // TabModelListObserver:
  // Covers cold start where a `TabModel` is registered with `TabModelList`
  // after `Activity.onResume()`.
  void OnTabModelAdded(TabModel* tab_model) override;
  void OnTabModelRemoved(TabModel* tab_model) override;

  // TabModelObserver:
  // Covers session restore or launch-intent tab creation setting the active
  // tab or activating the regular `TabModel` after window creation.
  void DidSelectTab(TabAndroid* tab) override;
  void DidAddTab(TabAndroid* tab, TabModel::TabLaunchType type) override;
  void OnDidActiveStateChange(TabModel& tab_model, bool active) override;
  void OnTabModelDestroyed(TabModel& tab_model) override;

 private:
  friend class base::NoDestructor<BackgroundOptInNotifier>;

  BackgroundOptInNotifier();
  ~BackgroundOptInNotifier() override;

  void StartObserving();
  void StopObserving();
  void RunScheduledForegroundNotify();
  bool Show();
  void Hide();

  base::RepeatingCallbackList<void(Event)> callbacks_;
  base::RepeatingCallback<bool()> show_cb_for_testing_;
  base::RepeatingClosure hide_cb_for_testing_;
  bool is_observing_ = false;
  bool foreground_notify_scheduled_ = false;
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
  base::ScopedMultiSourceObservation<TabModel, TabModelObserver>
      tab_model_observations_{this};
  base::WeakPtrFactory<BackgroundOptInNotifier> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_BACKGROUND_OPT_IN_NOTIFIER_ANDROID_H_
