// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_ACTIVE_PINNED_FOCUSED_TAB_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_ACTIVE_PINNED_FOCUSED_TAB_MANAGER_H_

#include "chrome/browser/glic/host/context/glic_focused_tab_manager_interface.h"
#include "chrome/browser/glic/host/context/glic_sharing_utils.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"

namespace glic {

class GlicSharingManager;

// Focused tab manager that combines "active" and "pinned" status. The
// active tab in the active browser is considered focused
// if it is both valid (see `GlicSharingUtils`) AND pinned for sharing.
//
// If both are not true, then it is considered the focused candidate.
//
// If the last active browser is not valid (see `GlicSharingUtils`), or there
// are no browsers, then there is neither a focused tab nor focused tab
// candidate.
//
// Note: makes no guarantees about de-duping of events, so subscribers should
// should handle de-duping in cases where that matters.
class GlicActivePinnedFocusedTabManager
    : public GlicFocusedTabManagerInterface {
 public:
  explicit GlicActivePinnedFocusedTabManager(
      Profile* profile,
      GlicSharingManager* sharing_manager);
  ~GlicActivePinnedFocusedTabManager() override;
  GlicActivePinnedFocusedTabManager(const GlicActivePinnedFocusedTabManager&) =
      delete;
  GlicActivePinnedFocusedTabManager& operator=(
      const GlicActivePinnedFocusedTabManager&) = delete;

  // GlicFocusedTabManagerInterface implementation.
  using FocusedTabChangedCallback =
      base::RepeatingCallback<void(const FocusedTabData&)>;
  base::CallbackListSubscription AddFocusedTabChangedCallback(
      FocusedTabChangedCallback callback) override;
  using FocusedTabDataChangedCallback =
      base::RepeatingCallback<void(const glic::mojom::TabData*)>;
  base::CallbackListSubscription AddFocusedTabDataChangedCallback(
      FocusedTabDataChangedCallback callback) override;
  bool IsTabFocused(tabs::TabHandle tab_handle) const override;
  FocusedTabData GetFocusedTabData() override;

 private:
  // Callback for active tab changes.
  void OnActiveTabChanged(tabs::TabInterface* active_tab);

  // Callback for tab pinning status changes.
  void OnTabPinningStatusChanged(tabs::TabInterface* tab, bool status);

  // Callback for tab data changes to active tab.
  void OnActiveTabDataChanged(TabDataChange change);

  // Updates the tab data observer to track the provided active tab.
  void UpdateActiveTabDataObserver(tabs::TabInterface* active_tab);

  // Updates the currently focused tab and notifies subscribers when changed.
  void UpdateFocusedTab();

  // Notifies subscribers of a change to the focused tab.
  void NotifyFocusedTabChanged(const FocusedTabData& focused_tab);

  // Notifies subscribers of a change to the focused tab data.
  void NotifyFocusedTabDataChanged(
      const glic::mojom::TabData* focused_tab_data);

  // Source of truth for pinned tabs.
  raw_ptr<GlicSharingManager> sharing_manager_;

  // TODO(b:444463509): refactor into a shared singleton for the profile.
  GlicActiveTabForProfileTracker active_tab_tracker_;

  // Callback for changes to the active tab.
  base::CallbackListSubscription active_tab_changed_subscription_;

  // Callback for changes to tab pinning status.
  base::CallbackListSubscription tab_pinning_status_changed_subscription_;

  // List of callbacks to fire when the focused tab changes.
  base::RepeatingCallbackList<void(const FocusedTabData&)>
      focused_tab_changed_callback_list_;

  // List of callbacks to fire when the focused tab data changes.
  base::RepeatingCallbackList<void(const glic::mojom::TabData*)>
      focused_tab_data_changed_callback_list_;

  // `TabDataObserver` for the active tab (if one exists).
  std::unique_ptr<TabDataObserver> active_tab_data_observer_;

  raw_ptr<Profile> profile_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_ACTIVE_PINNED_FOCUSED_TAB_MANAGER_H_
