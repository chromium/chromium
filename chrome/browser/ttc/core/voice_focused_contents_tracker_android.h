// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_VOICE_FOCUSED_CONTENTS_TRACKER_ANDROID_H_
#define CHROME_BROWSER_TTC_CORE_VOICE_FOCUSED_CONTENTS_TRACKER_ANDROID_H_

#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ttc/core/voice_focused_contents_tracker.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#include "components/tabs/public/tab_interface.h"

class Profile;

namespace ttc {

// Android implementation of VoiceFocusedContentsTracker, which binds to the
// active TabModel of the profile and follows window/model and tab switches
// through TabModelObserver and TabAndroid::Observer. In multi-window mode
// (where multiple regular TabModels report IsActiveModel() == true), switching
// window focus without selecting a tab does not fire a TabModelObserver
// callback, so the tracker rebinds when a tab is selected in the other window
// (DidSelectTab).
class VoiceFocusedContentsTrackerAndroid : public VoiceFocusedContentsTracker,
                                           public TabModelListObserver,
                                           public TabModelObserver,
                                           public TabAndroid::Observer {
 public:
  explicit VoiceFocusedContentsTrackerAndroid(Profile& profile);
  ~VoiceFocusedContentsTrackerAndroid() override;

  // VoiceFocusedContentsTracker:
  content::WebContents* GetActiveWebContents() const override;

  // TabModelListObserver:
  void OnTabModelAdded(TabModel* tab_model) override;
  void OnTabModelRemoved(TabModel* tab_model) override;

  // TabModelObserver:
  void DidSelectTab(TabAndroid* tab) override;
  void OnTabClosePending(const std::vector<TabAndroid*>& tabs) override;
  void TabClosureUndone(TabAndroid* tab) override;
  void OnTabCloseUndone(const std::vector<TabAndroid*>& tabs) override;
  void DidRemoveTabForClosure(TabAndroid* tab) override;
  void TabRemoved(TabAndroid* tab) override;
  void OnDidActiveStateChange(TabModel& tab_model, bool active) override;
  void OnTabModelDestroyed(TabModel& tab_model) override;

  // TabAndroid::Observer:
  void OnInitWebContents(TabAndroid* tab) override;

 private:
  void BindToTabModel(TabModel* tab_model);
  void MaybeBindTabModelForTab(TabAndroid* tab);
  void RemoveTabModel(TabModel* tab_model);
  void UpdateActiveState();
  void OnActiveTabWillDetach(tabs::TabInterface* tab,
                             tabs::TabInterface::DetachReason reason);
  void ResetActiveTabObservation();
  TabModel* FindActiveTabModel() const;

  const raw_ref<Profile> profile_;

  // The TabModel this tracker is currently bound to. Null if no TabModel of
  // `profile_` is active.
  raw_ptr<TabModel> bound_tab_model_ = nullptr;

  // Tracks the last reported active WebContents. `WasInvalidated()` detects
  // when a previously active WebContents is destroyed before a TabModel
  // removal callback runs.
  base::WeakPtr<content::WebContents> active_contents_;

  // Handle of an unloaded active tab that has emitted WillDetach(kDelete), so
  // intermediate UpdateActiveState() calls before removal do not re-observe it.
  tabs::TabHandle detaching_tab_handle_;

  base::ScopedObservation<TabAndroid, TabAndroid::Observer>
      active_tab_observation_{this};
  base::CallbackListSubscription active_tab_will_detach_subscription_;
  base::ScopedMultiSourceObservation<TabModel, TabModelObserver>
      tab_model_observations_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_VOICE_FOCUSED_CONTENTS_TRACKER_ANDROID_H_
