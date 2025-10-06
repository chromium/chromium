// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_active_pinned_focused_tab_manager.h"

#include "chrome/browser/glic/host/context/glic_sharing_utils.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"

namespace glic {

GlicActivePinnedFocusedTabManager::GlicActivePinnedFocusedTabManager(
    Profile* profile,
    GlicSharingManager* sharing_manager)
    : sharing_manager_(sharing_manager),
      active_tab_tracker_(profile),
      profile_(profile) {}

GlicActivePinnedFocusedTabManager::~GlicActivePinnedFocusedTabManager() =
    default;

base::CallbackListSubscription
GlicActivePinnedFocusedTabManager::AddFocusedTabChangedCallback(
    FocusedTabChangedCallback callback) {
  // Lazy-initialize upstream subscriptions here.
  if (!active_tab_changed_subscription_) {
    active_tab_changed_subscription_ =
        active_tab_tracker_.AddActiveTabChangedCallback(base::BindRepeating(
            &GlicActivePinnedFocusedTabManager::OnActiveTabChanged,
            base::Unretained(this)));
  }
  if (!tab_pinning_status_changed_subscription_ && sharing_manager_) {
    tab_pinning_status_changed_subscription_ =
        sharing_manager_->AddTabPinningStatusChangedCallback(
            base::BindRepeating(
                &GlicActivePinnedFocusedTabManager::OnTabPinningStatusChanged,
                base::Unretained(this)));
  }

  return focused_tab_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicActivePinnedFocusedTabManager::AddFocusedTabDataChangedCallback(
    FocusedTabDataChangedCallback callback) {
  // TODO(b:444463509): Implement focused tab data changed tracking.
  return focused_tab_data_changed_callback_list_.Add(std::move(callback));
}

bool GlicActivePinnedFocusedTabManager::IsTabFocused(
    tabs::TabHandle tab_handle) const {
  tabs::TabInterface* active_tab = active_tab_tracker_.GetActiveTab();
  if (active_tab && tab_handle == active_tab->GetHandle() &&
      sharing_manager_->IsTabPinned(tab_handle) &&
      IsTabValidForSharing(tab_handle.Get()->GetContents())) {
    return true;
  }

  return false;
}

FocusedTabData GlicActivePinnedFocusedTabManager::GetFocusedTabData() {
  tabs::TabInterface* active_tab = active_tab_tracker_.GetActiveTab();

  if (!active_tab) {
    return FocusedTabData(std::string("no focusable tab"), nullptr);
  }

  if (!sharing_manager_->IsTabPinned(active_tab->GetHandle()) ||
      !IsTabValidForSharing(active_tab->GetContents())) {
    return FocusedTabData(std::string("no focusable tab"), active_tab);
  }

  return FocusedTabData(active_tab);
}

void GlicActivePinnedFocusedTabManager::OnActiveTabChanged(
    tabs::TabInterface* active_tab) {
  UpdateFocusedTab();
}

void GlicActivePinnedFocusedTabManager::OnTabPinningStatusChanged(
    tabs::TabInterface* tab,
    bool status) {
  tabs::TabInterface* active_tab = active_tab_tracker_.GetActiveTab();

  if (active_tab && tab && active_tab == tab) {
    UpdateFocusedTab();
  }
}

void GlicActivePinnedFocusedTabManager::UpdateFocusedTab() {
  NotifyFocusedTabChanged(GetFocusedTabData());
}

void GlicActivePinnedFocusedTabManager::NotifyFocusedTabChanged(
    const FocusedTabData& focused_tab) {
  focused_tab_changed_callback_list_.Notify(focused_tab);
}

}  // namespace glic
