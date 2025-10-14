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
    UpdateActiveTabDataObserver(active_tab_tracker_.GetActiveTab());
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
  UpdateActiveTabDataObserver(active_tab);
  UpdateFocusedTab();
}

void GlicActivePinnedFocusedTabManager::OnActiveTabDataChanged(
    TabDataChange change) {
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

void GlicActivePinnedFocusedTabManager::UpdateActiveTabDataObserver(
    tabs::TabInterface* active_tab) {
  // TODO(b:444463509): consider handling TabChangedAt() events.
  active_tab_data_observer_ = std::make_unique<TabDataObserver>(
      active_tab ? active_tab->GetContents() : nullptr,
      base::BindRepeating(
          &GlicActivePinnedFocusedTabManager::OnActiveTabDataChanged,
          base::Unretained(this)));
}

void GlicActivePinnedFocusedTabManager::UpdateFocusedTab() {
  FocusedTabData focused_tab_data = GetFocusedTabData();
  NotifyFocusedTabChanged(focused_tab_data);
  NotifyFocusedTabDataChanged(
      CreateTabData(focused_tab_data.focus()
                        ? focused_tab_data.focus()->GetContents()
                        : nullptr)
          .get());
}

void GlicActivePinnedFocusedTabManager::NotifyFocusedTabChanged(
    const FocusedTabData& focused_tab) {
  focused_tab_changed_callback_list_.Notify(focused_tab);
}

void GlicActivePinnedFocusedTabManager::NotifyFocusedTabDataChanged(
    const glic::mojom::TabData* focused_tab_data) {
  focused_tab_data_changed_callback_list_.Notify(focused_tab_data);
}

}  // namespace glic
