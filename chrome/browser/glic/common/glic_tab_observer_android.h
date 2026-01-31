// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_COMMON_GLIC_TAB_OBSERVER_ANDROID_H_
#define CHROME_BROWSER_GLIC_COMMON_GLIC_TAB_OBSERVER_ANDROID_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/glic/common/glic_tab_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

// Stub implementation of GlicTabObserver for Android.
class GlicTabObserverAndroid : public GlicTabObserver,
                               public TabModelListObserver,
                               public TabModelObserver,
                               public TabAndroid::Observer {
 public:
  GlicTabObserverAndroid(Profile* profile, EventCallback callback);
  ~GlicTabObserverAndroid() override;

  // TabModelListObserver:
  void OnTabModelAdded(TabModel* model) override;
  void OnTabModelRemoved(TabModel* model) override;

  // TabModelObserver:
  void DidAddTab(TabAndroid* tab, TabModel::TabLaunchType type) override;
  void DidSelectTab(TabAndroid* tab, TabModel::TabSelectionType type) override;
  void TabClosureCommitted(TabAndroid* tab) override;
  void TabRemoved(TabAndroid* tab) override;
  void DidMoveTab(TabAndroid* tab, int new_index, int old_index) override;
  void OnTabClosePending(const std::vector<TabAndroid*>& tabs,
                         TabModel::TabClosingSource source) override;
  void TabClosureUndone(TabAndroid* tab) override;
  void OnTabCloseUndone(const std::vector<TabAndroid*>& tabs) override;

  // TabAndroid::Observer:
  void OnInitWebContents(TabAndroid* tab) override;

 private:
  class TabContentObserver;

  void OnTabChanged(TabAndroid* tab);
  void StartObservingTab(TabAndroid* tab);
  void StopObservingTab(TabAndroid* tab);

  tabs::TabInterface* GetLastActiveTab(TabModel* tab_model);

  void ResetLastActiveTab(TabModel* tab_model);

  raw_ptr<Profile> profile_;
  EventCallback callback_;

  // Tracks observations of individual TabModels.
  base::ScopedMultiSourceObservation<TabModel, TabModelObserver>
      observed_tab_models_{this};

  // Tracks observations of individual TabAndroids.
  base::ScopedMultiSourceObservation<TabAndroid, TabAndroid::Observer>
      observed_tabs_{this};

  // Maps TabModel* to the last active WebContents* within that TabModel.
  absl::flat_hash_map<TabModel*, raw_ptr<tabs::TabInterface>>
      last_active_tab_map_;

  absl::flat_hash_map<TabAndroid*, std::unique_ptr<TabContentObserver>>
      tab_observers_;
};

#endif  // CHROME_BROWSER_GLIC_COMMON_GLIC_TAB_OBSERVER_ANDROID_H_
