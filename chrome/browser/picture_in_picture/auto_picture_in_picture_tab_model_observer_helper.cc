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

namespace {

class TabAndroidDelegate
    : public AutoPictureInPictureTabModelObserverHelper::Delegate {
 public:
  bool IsTabDragging(content::WebContents* web_contents) const override {
    auto* tab = TabAndroid::FromWebContents(web_contents);
    return tab && tab->IsDragging();
  }
};

}  // namespace

AutoPictureInPictureTabModelObserverHelper::
    AutoPictureInPictureTabModelObserverHelper(
        content::WebContents* web_contents,
        ActivatedChangedCallback callback,
        std::unique_ptr<Delegate> delegate)
    : AutoPictureInPictureTabObserverHelperBase(web_contents,
                                                std::move(callback)),
      delegate_(delegate ? std::move(delegate)
                         : std::make_unique<TabAndroidDelegate>()) {}

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

  // Observe all existing TabModels to catch when the tab is added to any of
  // them. This is necessary because the tab might be moved between different
  // TabModels (e.g., when moving a tab to a new window), and we need to ensure
  // we are always observing the model that currently contains the tab to
  // receive updates like DidSelectTab.
  for (TabModel* model : TabModelList::models()) {
    model->AddObserver(this);
  }

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

  // Stop observing all TabModels.
  for (TabModel* model : TabModelList::models()) {
    model->RemoveObserver(this);
  }
  observed_tab_model_ = nullptr;
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
  // When a tab is dragged out of a window, two consecutive `DidSelectTab`
  // events are triggered: one for the primary tab being dragged and another
  // for the secondary tab that replaces it. To prevent the second event from
  // incorrectly triggering Auto-PiP by marking the primary tab as inactive,
  // we debounce the active status check if the observed tab is being dragged.
  bool is_tab_dragging = delegate_->IsTabDragging(GetObservedWebContents());

  // Update the observed tab active status only when it's not being dragged.
  ReevaluateObservedModelAndState(
      /* check_tab_activation= */ !is_tab_dragging);
}

void AutoPictureInPictureTabModelObserverHelper::TabRemoved(TabAndroid* tab) {
  ReevaluateObservedModelAndState();
}

void AutoPictureInPictureTabModelObserverHelper::WillCloseTab(TabAndroid* tab) {
  // If the tab is closing, we shouldn't trigger Auto-PiP. We pass false to
  // skip checking the activation status, effectively freezing the state until
  // the tab is destroyed. It's required because when closing a tab via
  // WillCloseTab, Clank doesn't immediately remove the tab from the TabModel.
  ReevaluateObservedModelAndState(false);
}

void AutoPictureInPictureTabModelObserverHelper::OnTabModelAdded(
    TabModel* model) {
  model->AddObserver(this);
}

void AutoPictureInPictureTabModelObserverHelper::OnTabModelRemoved(
    TabModel* model) {
  model->RemoveObserver(this);
  ReevaluateObservedModelAndState();
}

void AutoPictureInPictureTabModelObserverHelper::
    ReevaluateObservedModelAndState(bool check_tab_activation) {
  // Find the TabModel that currently contains our observed WebContents.
  TabModel* current_model =
      TabModelList::GetTabModelForWebContents(GetObservedWebContents());

  // If the TabModel has changed (e.g., tab moved between windows), update
  // our reference. We don't need to add/remove observers here because we
  // observe all models.
  if (current_model != observed_tab_model_) {
    observed_tab_model_ = current_model;
  }

  // Check for activation status.
  if (check_tab_activation) {
    UpdateIsTabActivated();
  }
}
void AutoPictureInPictureTabModelObserverHelper::UpdateIsTabActivated() {
  if (observed_tab_model_) {
    bool was_active = is_tab_activated_;

    // The observed tab is considered "active" if its model exists and its
    // WebContents is the currently active one in that model.
    is_tab_activated_ = GetActiveWebContents() == GetObservedWebContents();

    if (is_tab_activated_ != was_active) {
      RunCallback(is_tab_activated_);
    }
  }
}
