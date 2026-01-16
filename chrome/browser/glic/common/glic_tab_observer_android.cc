// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/glic_tab_observer_android.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

// For desktop, only certain operations are considered user initiated. This
// method will try to match those operations with Android specific actions.
TabCreationType ToTypeCreationType(TabModel::TabLaunchType type) {
  switch (type) {
    // Similar to desktop + button on tab group
    case TabModel::TabLaunchType::FROM_TAB_GROUP_UI:
    // Similar to + button for new tab
    case TabModel::TabLaunchType::FROM_CHROME_UI:
    // Similar to ctrl + T on desktop
    case TabModel::TabLaunchType::FROM_RECENT_TABS_FOREGROUND:
    case TabModel::TabLaunchType::FROM_RECENT_TABS:
      return TabCreationType::kUserInitiated;
    default:
      return TabCreationType::kUnknown;
  }
}

GlicTabObserverAndroid::GlicTabObserverAndroid(Profile* profile,
                                               EventCallback callback)
    : profile_(profile), callback_(std::move(callback)) {
  for (auto& model : TabModelList::models()) {
    OnTabModelAdded(model);
  }
  TabModelList::AddObserver(this);
}

GlicTabObserverAndroid::~GlicTabObserverAndroid() {
  TabModelList::RemoveObserver(this);
}

void GlicTabObserverAndroid::OnTabModelAdded(TabModel* model) {
  if (model->GetProfile() != profile_) {
    return;
  }

  observed_tab_models_.AddObservation(model);
  content::WebContents* active_contents = model->GetActiveWebContents();
  if (active_contents) {
    last_active_tab_map_[model] = TabAndroid::FromWebContents(active_contents);
  }
}

void GlicTabObserverAndroid::OnTabModelRemoved(TabModel* model) {
  if (observed_tab_models_.IsObservingSource(model)) {
    observed_tab_models_.RemoveObservation(model);
    last_active_tab_map_.erase(model);
  }
}

void GlicTabObserverAndroid::DidAddTab(TabAndroid* tab,
                                       TabModel::TabLaunchType type) {
  TabModel* tab_model = TabModelList::GetTabModelForTabAndroid(tab);
  CHECK(tab_model);

  tabs::TabInterface* old_tab = GetLastActiveTab(tab_model);
  callback_.Run(TabCreationEvent(tab, old_tab, ToTypeCreationType(type)));
}

void GlicTabObserverAndroid::DidSelectTab(TabAndroid* tab,
                                          TabModel::TabSelectionType type) {
  if (!tab) {
    return;
  }

  TabModel* tab_model = TabModelList::GetTabModelForTabAndroid(tab);
  CHECK(tab_model);

  tabs::TabInterface* old_tab = GetLastActiveTab(tab_model);
  last_active_tab_map_[tab_model] = tab;
  if (old_tab != static_cast<tabs::TabInterface*>(tab)) {
    callback_.Run(TabActivationEvent(tab, old_tab));
  }
}

void GlicTabObserverAndroid::TabRemoved(TabAndroid* tab) {
  ResetLastActiveTab(TabModelList::GetTabModelForTabAndroid(tab));
  callback_.Run(TabMutationEvent{});
}

void GlicTabObserverAndroid::DidMoveTab(TabAndroid* tab,
                                        int new_index,
                                        int old_index) {
  ResetLastActiveTab(TabModelList::GetTabModelForTabAndroid(tab));
  callback_.Run(TabMutationEvent{});
}

void GlicTabObserverAndroid::OnTabClosePending(
    const std::vector<TabAndroid*>& tabs,
    TabModel::TabClosingSource source) {
  for (auto* tab : tabs) {
    ResetLastActiveTab(TabModelList::GetTabModelForTabAndroid(tab));
  }
  callback_.Run(TabMutationEvent{});
}

tabs::TabInterface* GlicTabObserverAndroid::GetLastActiveTab(
    TabModel* tab_model) {
  auto iter = last_active_tab_map_.find(tab_model);
  if (iter != last_active_tab_map_.end()) {
    return iter->second;
  }
  return nullptr;
}

void GlicTabObserverAndroid::TabClosureUndone(TabAndroid* tab) {
  // Undo closing a tab is similar to add a tab.
  DidAddTab(tab, TabModel::TabLaunchType::FROM_CHROME_UI);
}

void GlicTabObserverAndroid::OnTabCloseUndone(
    const std::vector<TabAndroid*>& tabs) {
  for (auto* tab : tabs) {
    DidAddTab(tab, TabModel::TabLaunchType::FROM_CHROME_UI);
  }
}

void GlicTabObserverAndroid::ResetLastActiveTab(TabModel* tab_model) {
  if (!tab_model) {
    return;
  }

  last_active_tab_map_[tab_model] =
      TabAndroid::FromWebContents(tab_model->GetActiveWebContents());
}
