// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/voice_focused_contents_tracker_android.h"

#include <memory>

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/web_contents.h"

namespace ttc {

// static
std::unique_ptr<VoiceFocusedContentsTracker>
VoiceFocusedContentsTracker::Create(Profile& profile) {
  return std::make_unique<VoiceFocusedContentsTrackerAndroid>(profile);
}

VoiceFocusedContentsTrackerAndroid::VoiceFocusedContentsTrackerAndroid(
    Profile& profile)
    : profile_(profile) {
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

void VoiceFocusedContentsTrackerAndroid::OnTabModelAdded(TabModel* tab_model) {
  if (tab_model && tab_model->GetProfile() == &profile_.get()) {
    tab_model_observations_.AddObservation(tab_model);
    if (tab_model->IsActiveModel()) {
      BindToTabModel(tab_model);
    }
  }
}

void VoiceFocusedContentsTrackerAndroid::OnTabModelRemoved(
    TabModel* tab_model) {
  RemoveTabModel(tab_model);
}

void VoiceFocusedContentsTrackerAndroid::DidSelectTab(TabAndroid* tab) {
  if (bound_tab_model_ && bound_tab_model_->GetActiveTab() == tab) {
    has_active_contents_ = GetActiveWebContents() != nullptr;
    NotifyVoiceFocusedContentsChanged();
  }
}

void VoiceFocusedContentsTrackerAndroid::DidRemoveTabForClosure(
    TabAndroid* tab) {
  if (has_active_contents_ && !GetActiveWebContents()) {
    has_active_contents_ = false;
    NotifyVoiceFocusedContentsChanged();
  }
}

void VoiceFocusedContentsTrackerAndroid::TabRemoved(TabAndroid* tab) {
  if (has_active_contents_ && !GetActiveWebContents()) {
    has_active_contents_ = false;
    NotifyVoiceFocusedContentsChanged();
  }
}

void VoiceFocusedContentsTrackerAndroid::OnDidActiveStateChange(
    TabModel& tab_model,
    bool active) {
  if (active) {
    BindToTabModel(&tab_model);
  } else if (bound_tab_model_ == &tab_model) {
    BindToTabModel(FindActiveTabModel());
  }
}

void VoiceFocusedContentsTrackerAndroid::OnTabModelDestroyed(
    TabModel& tab_model) {
  RemoveTabModel(&tab_model);
}

void VoiceFocusedContentsTrackerAndroid::BindToTabModel(TabModel* tab_model) {
  if (bound_tab_model_ == tab_model) {
    return;
  }
  const bool had_active_contents = GetActiveWebContents() != nullptr;
  bound_tab_model_ = tab_model;
  has_active_contents_ = GetActiveWebContents() != nullptr;
  if (had_active_contents || has_active_contents_) {
    NotifyVoiceFocusedContentsChanged();
  }
}

void VoiceFocusedContentsTrackerAndroid::RemoveTabModel(TabModel* tab_model) {
  if (!tab_model) {
    return;
  }
  if (tab_model_observations_.IsObservingSource(tab_model)) {
    tab_model_observations_.RemoveObservation(tab_model);
  }
  if (bound_tab_model_ == tab_model) {
    BindToTabModel(FindActiveTabModel());
  }
}

TabModel* VoiceFocusedContentsTrackerAndroid::FindActiveTabModel() const {
  for (TabModel* tab_model : TabModelList::models()) {
    if (tab_model && tab_model->GetProfile() == &profile_.get() &&
        tab_model->IsActiveModel() &&
        tab_model_observations_.IsObservingSource(tab_model)) {
      return tab_model;
    }
  }
  return nullptr;
}

}  // namespace ttc
