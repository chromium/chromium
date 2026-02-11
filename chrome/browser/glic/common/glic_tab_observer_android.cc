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
    case TabModel::TabLaunchType::FROM_LINK:
    case TabModel::TabLaunchType::FROM_LINK_CREATING_NEW_WINDOW:
    case TabModel::TabLaunchType::FROM_LONGPRESS_FOREGROUND:
    case TabModel::TabLaunchType::FROM_LONGPRESS_BACKGROUND:
      return TabCreationType::kFromLink;
    default:
      return TabCreationType::kUnknown;
  }
}

class GlicTabObserverAndroid::TabContentObserver
    : public content::WebContentsObserver {
 public:
  TabContentObserver(GlicTabObserverAndroid* owner, TabAndroid* tab)
      : content::WebContentsObserver(tab->web_contents()),
        owner_(owner),
        tab_(tab) {}

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    owner_->OnTabChanged(tab_);
  }
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    owner_->OnTabChanged(tab_);
  }
  void TitleWasSet(content::NavigationEntry* entry) override {
    owner_->OnTabChanged(tab_);
  }
  void DidStopLoading() override { owner_->OnTabChanged(tab_); }

 private:
  raw_ptr<GlicTabObserverAndroid> owner_;
  raw_ptr<TabAndroid> tab_;
};

GlicTabObserverAndroid::GlicTabObserverAndroid(Profile* profile,
                                               EventCallback callback)
    : profile_(profile), callback_(std::move(callback)) {
  for (auto& model : TabModelList::models()) {
    OnTabModelAdded(model);
  }
  TabModelList::AddObserver(this);
}

void GlicTabObserverAndroid::OnTabChanged(TabAndroid* tab) {
  callback_.Run(TabMutationEvent{});
}

void GlicTabObserverAndroid::StartObservingTab(TabAndroid* tab) {
  if (!observed_tabs_.IsObservingSource(tab)) {
    observed_tabs_.AddObservation(tab);
  }

  content::WebContents* web_contents = tab->web_contents();
  // `web_contents` may be null if a tab has been frozen in the
  // background. It can also be null temporarily during reparenting.
  // We will get called via `OnInitWebContents` when it becomes available.
  if (!web_contents) {
    return;
  }

  // If we are already observing the correct WebContents, do nothing.
  if (auto it = tab_observers_.find(tab); it != tab_observers_.end()) {
    if (it->second->web_contents() == web_contents) {
      return;
    }
  }

  // Create or replace the observer.
  tab_observers_[tab] = std::make_unique<TabContentObserver>(this, tab);
}

void GlicTabObserverAndroid::StopObservingTab(TabAndroid* tab) {
  tab_observers_.erase(tab);
  if (observed_tabs_.IsObservingSource(tab)) {
    observed_tabs_.RemoveObservation(tab);
  }
}

void GlicTabObserverAndroid::OnInitWebContents(TabAndroid* tab) {
  StartObservingTab(tab);
}

GlicTabObserverAndroid::~GlicTabObserverAndroid() {
  TabModelList::RemoveObserver(this);
}

void GlicTabObserverAndroid::OnTabModelAdded(TabModel* model) {
  if (model->GetProfile() != profile_ ||
      model->GetTabModelType() != TabModel::TabModelType::kStandard) {
    return;
  }

  observed_tab_models_.AddObservation(model);
  content::WebContents* active_contents = model->GetActiveWebContents();
  if (active_contents) {
    last_active_tab_map_[model] = TabAndroid::FromWebContents(active_contents);
  }

  for (int i = 0; i < model->GetTabCount(); ++i) {
    StartObservingTab(model->GetTabAt(i));
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
  StartObservingTab(tab);
  TabModel* tab_model = TabModelList::GetTabModelForTabAndroid(tab);
  CHECK(tab_model);

  tabs::TabInterface* old_tab = GetLastActiveTab(tab_model);
  tabs::TabInterface* opener = tab_model->GetOpenerForTab(tab->GetHandle());
  callback_.Run(
      TabCreationEvent{tab, old_tab, opener, ToTypeCreationType(type)});
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

void GlicTabObserverAndroid::TabClosureCommitted(TabAndroid* tab) {
  ResetLastActiveTab(TabModelList::GetTabModelForTabAndroid(tab));
  callback_.Run(TabMutationEvent{});
}

void GlicTabObserverAndroid::TabRemoved(TabAndroid* tab) {
  StopObservingTab(tab);
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
