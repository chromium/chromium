// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_VOICE_FOCUSED_CONTENTS_TRACKER_ANDROID_H_
#define CHROME_BROWSER_TTC_CORE_VOICE_FOCUSED_CONTENTS_TRACKER_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/ttc/core/voice_focused_contents_tracker.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"

class Profile;
class TabAndroid;

namespace ttc {

// Note: This class currently has zero test coverage. Consider it a skeleton
// which *must* be built out in order to handle tab and window switches
// robustly.
//
// Android implementation of VoiceFocusedContentsTracker, which binds to the
// active TabModel of the profile and follows window/model switches through
// TabModelObserver.
class VoiceFocusedContentsTrackerAndroid : public VoiceFocusedContentsTracker,
                                           public TabModelListObserver,
                                           public TabModelObserver {
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
  void DidRemoveTabForClosure(TabAndroid* tab) override;
  void TabRemoved(TabAndroid* tab) override;
  void OnDidActiveStateChange(TabModel& tab_model, bool active) override;
  void OnTabModelDestroyed(TabModel& tab_model) override;

 private:
  void BindToTabModel(TabModel* tab_model);
  void RemoveTabModel(TabModel* tab_model);
  TabModel* FindActiveTabModel() const;

  const raw_ref<Profile> profile_;

  // The TabModel this tracker is currently bound to. Null if no TabModel of
  // `profile_` is active.
  raw_ptr<TabModel> bound_tab_model_ = nullptr;

  bool has_active_contents_ = false;

  base::ScopedMultiSourceObservation<TabModel, TabModelObserver>
      tab_model_observations_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_VOICE_FOCUSED_CONTENTS_TRACKER_ANDROID_H_
