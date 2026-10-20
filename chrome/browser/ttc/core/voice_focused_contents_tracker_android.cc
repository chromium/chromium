// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/voice_focused_contents_tracker_android.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "content/public/browser/web_contents.h"

namespace ttc {

// static
std::unique_ptr<VoiceFocusedContentsTracker>
VoiceFocusedContentsTracker::Create(Profile& profile) {
  return std::make_unique<VoiceFocusedContentsTrackerAndroid>(
      profile, ProfileBrowserCollection::GetForProfile(&profile));
}

VoiceFocusedContentsTrackerAndroid::VoiceFocusedContentsTrackerAndroid(
    Profile& profile,
    BrowserCollection* browsers)
    : profile_(profile) {
  if (browsers) {
    browser_collection_observation_.Observe(browsers);
  }
  TabModelList::AddObserver(this);
  for (TabModel* tab_model : TabModelList::models()) {
    OnTabModelAdded(tab_model);
  }
}

VoiceFocusedContentsTrackerAndroid::~VoiceFocusedContentsTrackerAndroid() {
  TabModelList::RemoveObserver(this);
}

content::WebContents* VoiceFocusedContentsTrackerAndroid::GetActiveWebContents()
    const {
  return bound_tab_model_ ? bound_tab_model_->GetActiveWebContents() : nullptr;
}

void VoiceFocusedContentsTrackerAndroid::OnBrowserClosed(
    BrowserWindowInterface* browser) {
  UpdateBoundTabModel();
}

void VoiceFocusedContentsTrackerAndroid::OnBrowserActivated(
    BrowserWindowInterface* browser) {
  UpdateBoundTabModel();
}

void VoiceFocusedContentsTrackerAndroid::OnTabModelAdded(TabModel* tab_model) {
  // Only standard models back a window with live tabs. Headless models (for
  // persisted windows that aren't open, registered asynchronously on desktop
  // Android) and archived models also report IsActiveModel(), but their tabs
  // never have a WebContents, so binding to them loses the active tab.
  if (!tab_model || tab_model->GetProfile() != &profile_.get() ||
      tab_model->GetTabModelType() != TabModel::TabModelType::kStandard) {
    return;
  }
  if (!tab_model_observations_.IsObservingSource(tab_model)) {
    tab_model_observations_.AddObservation(tab_model);
  }
  UpdateBoundTabModel();
}

void VoiceFocusedContentsTrackerAndroid::OnTabModelRemoved(
    TabModel* tab_model) {
  RemoveTabModel(tab_model);
}

void VoiceFocusedContentsTrackerAndroid::DidSelectTab(TabAndroid* tab) {
  // TabModelSelectorBase.initialize() activates the first TabModel without
  // firing OnDidActiveStateChange, so it may first become bindable here.
  UpdateBoundTabModel();
  UpdateActiveState();
}

void VoiceFocusedContentsTrackerAndroid::OnTabClosePending(
    const std::vector<TabAndroid*>& tabs) {
  UpdateActiveState();
}

void VoiceFocusedContentsTrackerAndroid::TabClosureUndone(TabAndroid* tab) {
  UpdateActiveState();
}

void VoiceFocusedContentsTrackerAndroid::OnTabCloseUndone(
    const std::vector<TabAndroid*>& tabs) {
  UpdateActiveState();
}

void VoiceFocusedContentsTrackerAndroid::DidRemoveTabForClosure(
    TabAndroid* tab) {
  UpdateActiveState();
}

void VoiceFocusedContentsTrackerAndroid::TabRemoved(TabAndroid* tab) {
  UpdateActiveState();
}

void VoiceFocusedContentsTrackerAndroid::OnDidActiveStateChange(
    TabModel& tab_model,
    bool active) {
  UpdateBoundTabModel();
}

void VoiceFocusedContentsTrackerAndroid::OnTabModelDestroyed(
    TabModel& tab_model) {
  RemoveTabModel(&tab_model);
}

void VoiceFocusedContentsTrackerAndroid::OnInitWebContents(TabAndroid* tab) {
  UpdateActiveState();
}

void VoiceFocusedContentsTrackerAndroid::UpdateBoundTabModel() {
  TabModel* tab_model = FindFocusedTabModel();
  if (bound_tab_model_ == tab_model) {
    return;
  }
  bound_tab_model_ = tab_model;
  UpdateActiveState();
}

void VoiceFocusedContentsTrackerAndroid::RemoveTabModel(TabModel* tab_model) {
  if (!tab_model) {
    return;
  }
  if (tab_model_observations_.IsObservingSource(tab_model)) {
    tab_model_observations_.RemoveObservation(tab_model);
  }
  if (bound_tab_model_ == tab_model) {
    UpdateBoundTabModel();
  }
}

void VoiceFocusedContentsTrackerAndroid::UpdateActiveState() {
  TabAndroid* active_tab =
      bound_tab_model_
          ? TabAndroid::FromTabInterface(bound_tab_model_->GetActiveTab())
          : nullptr;
  if (!active_tab || active_tab->GetHandle() != detaching_tab_handle_) {
    detaching_tab_handle_ = tabs::TabHandle::Null();
  }
  content::WebContents* current_contents =
      bound_tab_model_ ? bound_tab_model_->GetActiveWebContents() : nullptr;

  ResetActiveTabObservation();
  if (active_tab && active_tab->GetHandle() != detaching_tab_handle_ &&
      !current_contents) {
    // Watch for deferred WebContents initialization, and detach before
    // ~TabAndroid if TabCollectionTabModelImpl.destroy() tears down tabs prior
    // to OnTabModelDestroyed.
    active_tab_observation_.Observe(active_tab);
    active_tab_will_detach_subscription_ =
        active_tab->RegisterWillDetach(base::BindRepeating(
            &VoiceFocusedContentsTrackerAndroid::OnActiveTabWillDetach,
            base::Unretained(this)));
  }

  if (current_contents != active_contents_.get() ||
      active_contents_.WasInvalidated()) {
    active_contents_ =
        current_contents ? current_contents->GetWeakPtr() : nullptr;
    NotifyVoiceFocusedContentsChanged();
  }
}

void VoiceFocusedContentsTrackerAndroid::OnActiveTabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  // A detaching tab is leaving the bound TabModel for any reason, so stop
  // observing it; if it is re-inserted and selected, DidSelectTab re-observes.
  // A tab detached for reparenting and destroyed before re-insertion never
  // sends WillDetach(kDelete), so waiting for kDelete would leave a dangling
  // TabAndroid::Observer registration.
  if (reason == tabs::TabInterface::DetachReason::kDelete) {
    detaching_tab_handle_ = tab->GetHandle();
  }
  ResetActiveTabObservation();
}

void VoiceFocusedContentsTrackerAndroid::ResetActiveTabObservation() {
  active_tab_will_detach_subscription_ = {};
  active_tab_observation_.Reset();
}

TabModel* VoiceFocusedContentsTrackerAndroid::FindFocusedTabModel() {
  BrowserWindowInterface* browser =
      browser_collection_observation_.IsObserving()
          ? browser_collection_observation_.GetSource()->GetLastActiveBrowser()
          : nullptr;
  if (!browser) {
    return nullptr;
  }
  for (TabModel* tab_model : TabModelList::models()) {
    if (tab_model_observations_.IsObservingSource(tab_model) &&
        tab_model->IsActiveModel() &&
        tab_model->GetSessionId() == browser->GetSessionID()) {
      return tab_model;
    }
  }
  return nullptr;
}

}  // namespace ttc
