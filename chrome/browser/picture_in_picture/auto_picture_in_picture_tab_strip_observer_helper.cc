// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_strip_observer_helper.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"

AutoPictureInPictureTabStripObserverHelper::
    AutoPictureInPictureTabStripObserverHelper(
        const content::WebContents* web_contents,
        ActivatedChangedCallback callback)
    : web_contents_(web_contents), callback_(std::move(callback)) {}

AutoPictureInPictureTabStripObserverHelper::
    ~AutoPictureInPictureTabStripObserverHelper() {
  StopObserving();
}

void AutoPictureInPictureTabStripObserverHelper::StartObserving() {
  if (is_observing_) {
    return;
  }
  is_observing_ = true;

  auto* tab_strip_model = GetCurrentTabStripModel();
  UpdateIsTabActivated(tab_strip_model);

  ObserveTabStripModel(tab_strip_model);
}

void AutoPictureInPictureTabStripObserverHelper::StopObserving() {
  if (!is_observing_) {
    return;
  }
  is_observing_ = false;

  if (observed_tab_strip_model_) {
    observed_tab_strip_model_->RemoveObserver(this);
    observed_tab_strip_model_ = nullptr;
  }
  browser_tab_strip_tracker_.reset();
}

void AutoPictureInPictureTabStripObserverHelper::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // The BrowserTabStripTracker will fire this during initial processing, but we
  // want to ignore those.
  if (browser_tab_strip_tracker_ &&
      browser_tab_strip_tracker_->is_processing_initial_browsers()) {
    return;
  }

  auto* current_tab_strip_model = GetCurrentTabStripModel();
  if (current_tab_strip_model != observed_tab_strip_model_) {
    ObserveTabStripModel(current_tab_strip_model);
  }

  // We should not try to update our state on an insertion that does not also
  // change the selection, since the selection change will follow immediately
  // and we should not trigger based on this intermediate state. Note that there
  // is a technically possible case where a foreground tab gets inserted into a
  // different tabstrip as a background tab without ever changing the currently
  // selected tab. In this particular case, we will not trigger the callback.
  // Ideally, we would be able to distinguish this case from the case where the
  // selection will change, but that's not currently feasible and given how
  // easily/often the insert-and-change case happens and how infrequently the
  // insert-into-background-from-foreground case comes up, not triggering in
  // that case is a reasonable tradeoff.
  if (change.type() == TabStripModelChange::kInserted &&
      !selection.active_tab_changed()) {
    return;
  }

  const bool old_is_tab_activated = is_tab_activated_;
  UpdateIsTabActivated(current_tab_strip_model);
  if (is_tab_activated_ == old_is_tab_activated) {
    return;
  }

  callback_.Run(is_tab_activated_);
}

void AutoPictureInPictureTabStripObserverHelper::UpdateIsTabActivated(
    const TabStripModel* tab_strip_model) {
  // If the tab is not currently in a tab strip model (e.g. it is currently
  // being dragged between tabstrips), then we don't want to update the value
  // until it's been placed back into a tabstrip to prevent triggering the
  // callback unnecessarily.
  if (tab_strip_model) {
    // If there is not currently a selected tab, then the tabstrip is still
    // starting up and we should not change our state.
    if (tab_strip_model->active_index() == TabStripModel::kNoTab) {
      return;
    }

    is_tab_activated_ =
        tab_strip_model->GetActiveWebContents() == web_contents_;
  }
}

void AutoPictureInPictureTabStripObserverHelper::ObserveTabStripModel(
    TabStripModel* tab_strip_model) {
  if (observed_tab_strip_model_) {
    observed_tab_strip_model_->RemoveObserver(this);
    observed_tab_strip_model_ = nullptr;
  }
  if (tab_strip_model) {
    browser_tab_strip_tracker_.reset();
    observed_tab_strip_model_ = tab_strip_model;
    observed_tab_strip_model_->AddObserver(this);
    return;
  }

  // If we don't currently have a tab strip model, then we need to watch all
  // browsers until one of them has `web_contents_`. We can get into this state
  // when tabs are being dragged between tabstrips.
  CHECK(!browser_tab_strip_tracker_);
  browser_tab_strip_tracker_ =
      std::make_unique<BrowserTabStripTracker>(this, nullptr);
  browser_tab_strip_tracker_->Init();
}

TabStripModel*
AutoPictureInPictureTabStripObserverHelper::GetCurrentTabStripModel() const {
  // If this WebContents isn't in a normal browser window, then auto
  // picture-in-picture is not supported.
  auto* browser = chrome::FindBrowserWithTab(web_contents_);
  if (!browser || !browser->is_type_normal()) {
    return nullptr;
  }
  return browser->tab_strip_model();
}

content::WebContents*
AutoPictureInPictureTabStripObserverHelper::GetActiveWebContents() const {
  if (!observed_tab_strip_model_) {
    return nullptr;
  }

  return observed_tab_strip_model_->GetActiveWebContents();
}
