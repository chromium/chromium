// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_triggering/glic_background_opt_in_notifier_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/glic/android/jni_headers/GlicExperimentalOptInBridge_jni.h"

namespace glic {

// static
BackgroundOptInNotifier& BackgroundOptInNotifier::Get() {
  static base::NoDestructor<BackgroundOptInNotifier> instance;
  return *instance;
}

BackgroundOptInNotifier::BackgroundOptInNotifier() {
  callbacks_.set_removal_callback(base::BindRepeating(
      [](BackgroundOptInNotifier* self) {
        if (self->callbacks_.empty()) {
          self->StopObserving();
          self->Hide();
        }
      },
      base::Unretained(this)));
}

BackgroundOptInNotifier::~BackgroundOptInNotifier() = default;

base::CallbackListSubscription BackgroundOptInNotifier::Subscribe(
    base::RepeatingCallback<void(Event)> cb) {
  if (callbacks_.empty()) {
    if (!Show()) {
      return {};
    }
    StartObserving();
  }
  return callbacks_.Add(std::move(cb));
}

void BackgroundOptInNotifier::Notify(Event event) {
  callbacks_.Notify(event);
}

void BackgroundOptInNotifier::ScheduleForegroundNotification() {
  if (foreground_notify_scheduled_ || callbacks_.empty()) {
    return;
  }
  foreground_notify_scheduled_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&BackgroundOptInNotifier::RunScheduledForegroundNotify,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundOptInNotifier::SetCallbacksForTesting(
    base::RepeatingCallback<bool()> show_cb,
    base::RepeatingClosure hide_cb) {
  show_cb_for_testing_ = std::move(show_cb);
  hide_cb_for_testing_ = std::move(hide_cb);
}

void BackgroundOptInNotifier::OnBrowserCreated(
    BrowserWindowInterface* browser) {
  ScheduleForegroundNotification();
}

void BackgroundOptInNotifier::OnBrowserActivated(
    BrowserWindowInterface* browser) {
  ScheduleForegroundNotification();
}

void BackgroundOptInNotifier::OnTabModelAdded(TabModel* tab_model) {
  if (tab_model && !tab_model_observations_.IsObservingSource(tab_model)) {
    tab_model_observations_.AddObservation(tab_model);
  }
  ScheduleForegroundNotification();
}

void BackgroundOptInNotifier::OnTabModelRemoved(TabModel* tab_model) {
  if (tab_model && tab_model_observations_.IsObservingSource(tab_model)) {
    tab_model_observations_.RemoveObservation(tab_model);
  }
}

void BackgroundOptInNotifier::DidSelectTab(TabAndroid* tab) {
  ScheduleForegroundNotification();
}

void BackgroundOptInNotifier::DidAddTab(TabAndroid* tab,
                                        TabModel::TabLaunchType type) {
  ScheduleForegroundNotification();
}

void BackgroundOptInNotifier::OnDidActiveStateChange(TabModel& tab_model,
                                                     bool active) {
  if (active) {
    ScheduleForegroundNotification();
  }
}

void BackgroundOptInNotifier::OnTabModelDestroyed(TabModel& tab_model) {
  if (tab_model_observations_.IsObservingSource(&tab_model)) {
    tab_model_observations_.RemoveObservation(&tab_model);
  }
}

void BackgroundOptInNotifier::StartObserving() {
  if (is_observing_) {
    return;
  }
  is_observing_ = true;
  if (g_browser_process && g_browser_process->GetFeatures()) {
    if (auto* collection = GlobalBrowserCollection::GetInstance()) {
      browser_collection_observation_.Observe(collection);
    }
  }
  TabModelList::AddGlobalObserver(this);
  TabModelList::ForEachAll([this](TabModel* tab_model) {
    if (tab_model && !tab_model_observations_.IsObservingSource(tab_model)) {
      tab_model_observations_.AddObservation(tab_model);
    }
    return true;
  });
}

void BackgroundOptInNotifier::StopObserving() {
  if (!is_observing_) {
    return;
  }
  is_observing_ = false;
  foreground_notify_scheduled_ = false;
  weak_ptr_factory_.InvalidateWeakPtrs();
  tab_model_observations_.RemoveAllObservations();
  TabModelList::RemoveGlobalObserver(this);
  browser_collection_observation_.Reset();
}

void BackgroundOptInNotifier::RunScheduledForegroundNotify() {
  foreground_notify_scheduled_ = false;
  if (!callbacks_.empty()) {
    Notify(Event::kForegrounded);
  }
}

bool BackgroundOptInNotifier::Show() {
  if (show_cb_for_testing_) {
    return show_cb_for_testing_.Run();
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_GlicExperimentalOptInBridge_show(env);
}

void BackgroundOptInNotifier::Hide() {
  if (hide_cb_for_testing_) {
    hide_cb_for_testing_.Run();
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_GlicExperimentalOptInBridge_hide(env);
}

// Covers a warm activity start or resume where the native browser window and
// active tab already exist when `Activity.onStart()` / `Activity.onResume()`
// fires.
static void JNI_GlicExperimentalOptInBridge_OnActivityStartedOrResumed(
    JNIEnv* env) {
  BackgroundOptInNotifier::Get().ScheduleForegroundNotification();
}

static void JNI_GlicExperimentalOptInBridge_OnOptInCancelled(JNIEnv* env) {
  BackgroundOptInNotifier::Get().Notify(
      BackgroundOptInNotifier::Event::kCancelled);
}

}  // namespace glic

DEFINE_JNI(GlicExperimentalOptInBridge)
