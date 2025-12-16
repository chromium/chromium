// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_model_observer_helper.h"

#include "base/notimplemented.h"
#include "chrome/browser/android/tab_android.h"  // nogncheck crbug.com/413572035
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/web_contents.h"

// static
std::unique_ptr<AutoPictureInPictureTabObserverHelperBase>
AutoPictureInPictureTabObserverHelperBase::Create(
    content::WebContents* web_contents,
    ActivatedChangedCallback callback) {
  return std::make_unique<AutoPictureInPictureTabModelObserverHelper>(
      web_contents, std::move(callback));
}

AutoPictureInPictureTabModelObserverHelper::
    AutoPictureInPictureTabModelObserverHelper(
        content::WebContents* web_contents,
        ActivatedChangedCallback callback)
    : AutoPictureInPictureTabObserverHelperBase(web_contents,
                                                std::move(callback)) {}

AutoPictureInPictureTabModelObserverHelper::
    ~AutoPictureInPictureTabModelObserverHelper() {
  StopObserving();
}

void AutoPictureInPictureTabModelObserverHelper::StartObserving() {
  if (is_observing_) {
    return;
  }
  is_observing_ = true;

  // Start observing the list of TabModels.
  TabModelList::AddObserver(this);

  // Find the correct model to observe and set the initial activation state.
  ReevaluateObservedModelAndState();
}

void AutoPictureInPictureTabModelObserverHelper::StopObserving() {
  if (!is_observing_) {
    return;
  }
  is_observing_ = false;

  // Remove the global TabModelList observer.
  TabModelList::RemoveObserver(this);

  // If we are observing a specific TabModel, remove the observer from it.
  if (observed_tab_model_) {
    observed_tab_model_->RemoveObserver(this);
    observed_tab_model_ = nullptr;
  }
}

// The following callbacks are for events that might change the active tab
// or the state of the observed tab model. We re-evaluate our state in each
// case.
content::WebContents*
AutoPictureInPictureTabModelObserverHelper::GetActiveWebContents() const {
  if (!observed_tab_model_) {
    return nullptr;
  }
  return observed_tab_model_->GetActiveWebContents();
}

void AutoPictureInPictureTabModelObserverHelper::DidSelectTab(
    TabAndroid* tab,
    TabModel::TabSelectionType type) {
  ReevaluateObservedModelAndState();
}

void AutoPictureInPictureTabModelObserverHelper::TabRemoved(TabAndroid* tab) {
  ReevaluateObservedModelAndState();
}

void AutoPictureInPictureTabModelObserverHelper::WillCloseTab(TabAndroid* tab) {
  ReevaluateObservedModelAndState();
}

void AutoPictureInPictureTabModelObserverHelper::OnTabModelAdded(
    TabModel* model) {
  ReevaluateObservedModelAndState();
}

void AutoPictureInPictureTabModelObserverHelper::OnTabModelRemoved(
    TabModel* model) {
  ReevaluateObservedModelAndState();
}

void AutoPictureInPictureTabModelObserverHelper::
    ReevaluateObservedModelAndState() {
  // Find the TabModel that currently contains our observed WebContents.
  TabModel* current_model =
      TabModelList::GetTabModelForWebContents(GetObservedWebContents());

  // If the TabModel has changed (e.g., tab moved between windows), update
  // observers.
  if (current_model != observed_tab_model_) {
    if (observed_tab_model_) {
      observed_tab_model_->RemoveObserver(this);
    }
    observed_tab_model_ = current_model;
    if (observed_tab_model_) {
      observed_tab_model_->AddObserver(this);
    }
  }

  // Check for activation status.
  UpdateIsTabActivated();
}

void AutoPictureInPictureTabModelObserverHelper::UpdateIsTabActivated() {
  bool was_active = is_tab_activated_;
  content::WebContents* active_wc = GetActiveWebContents();

  // The observed tab is considered "active" if its model exists and its
  // WebContents is the currently active one in that model.
  is_tab_activated_ =
      (observed_tab_model_ && active_wc == GetObservedWebContents());

  if (is_tab_activated_ != was_active) {
    RunCallback(is_tab_activated_);
  }
}
