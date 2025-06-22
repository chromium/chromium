// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PINNED_TAB_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PINNED_TAB_MANAGER_H_

#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

class GlicSharingManagerImpl;

// Manages a collection of tabs that have been selected to be shared.
class GlicPinnedTabManager {
 public:
  explicit GlicPinnedTabManager(GlicSharingManagerImpl* sharing_manager);
  ~GlicPinnedTabManager();

  // Registers a callback to be invoked when the collection of pinned tabs
  // changes.
  using PinnedTabsChangedCallback =
      base::RepeatingCallback<void(const std::vector<content::WebContents*>&)>;
  base::CallbackListSubscription AddPinnedTabsChangedCallback(
      PinnedTabsChangedCallback callback);

  // Registers a callback to be invoked when the pinned status of a tab changes.
  using TabPinningStatusChangedCallback =
      base::RepeatingCallback<void(tabs::TabInterface*, bool)>;
  base::CallbackListSubscription AddTabPinningStatusChangedCallback(
      TabPinningStatusChangedCallback callback);

  // Registers a callback to be invoked when the TabData for a pinned tab is
  // changed.
  using PinnedTabDataChangedCallback =
      base::RepeatingCallback<void(const glic::mojom::TabData*)>;
  base::CallbackListSubscription AddPinnedTabDataChangedCallback(
      PinnedTabDataChangedCallback callback);

  // Pins the specified tabs. If we are only able to pin `n` tabs within the
  // limit, the first `n` tabs from this collection will be pinned and we will
  // return false (to indicate that it was not fully successful). If any of the
  // tab handles correspond to a tab that either doesn't exist or is already
  // pinned, it will be skipped and we will similarly return false to indicate
  // that the function was not fully successful.
  bool PinTabs(base::span<const tabs::TabHandle> tab_handles);

  // Unins the specified tabs. If any of the tab handles correspond to a tab
  // that either doesn't exist or is not pinned, it will be skipped and we will
  // similarly return false to indicate that the function was not fully
  // successful.
  bool UnpinTabs(base::span<const tabs::TabHandle> tab_handles);

  // Unpins all pinned tabs.
  void UnpinAllTabs();

  // Sets the limit on the number of pinned tabs. Returns the effective number
  // of pinned tabs. Can differ due to supporting fewer tabs than requested or
  // having more tabs currently pinned than requested.
  uint32_t SetMaxPinnedTabs(uint32_t max_pinned_tabs);

  // Gets the limit on the number of pinned tabs.
  uint32_t GetMaxPinnedTabs() const;

  // Gets the current number of pinned tabs.
  uint32_t GetNumPinnedTabs() const;

  // Returns true if the tab is in the pinned collection.
  bool IsTabPinned(tabs::TabHandle tab_handle) const;

  // Fetches the current list of pinned tabs.
  std::vector<content::WebContents*> GetPinnedTabs() const;

 private:
  class PinnedTabObserver;
  struct PinnedTabEntry {
    PinnedTabEntry(tabs::TabHandle tab_handle,
                   std::unique_ptr<PinnedTabObserver> tab_observer);
    ~PinnedTabEntry();
    PinnedTabEntry(PinnedTabEntry&& other);
    PinnedTabEntry& operator=(PinnedTabEntry&& other);
    PinnedTabEntry(const PinnedTabEntry&) = delete;
    PinnedTabEntry& operator=(const PinnedTabEntry&) = delete;
    tabs::TabHandle tab_handle;
    std::unique_ptr<PinnedTabObserver> tab_observer;
  };

  // Sends an update to the web client with the full set of pinned tabs.
  void NotifyPinnedTabsChanged();

  // Returns the entry corresponding to the given tab_handle, if it exists.
  const PinnedTabEntry* GetPinnedTabEntry(tabs::TabHandle tab_handle) const;

  // Returns true if the tab is in the pinned collection.
  bool IsTabPinned(int tab_id) const;

  // Called by the PinnedTabObserver.
  void OnTabWillClose(tabs::TabHandle tab_handles);

  // Called by the PinnedTabObserver.
  void OnTabDataChanged(tabs::TabHandle tab_handle, glic::mojom::TabDataPtr);

  // List of callbacks to invoke when the collection of pinned tabs changes
  // (including changes to metadata).
  base::RepeatingCallbackList<void(const std::vector<content::WebContents*>&)>
      pinned_tabs_changed_callback_list_;

  // List of callbacks to invoke when the tab data for a pinned tab changes.
  base::RepeatingCallbackList<void(const glic::mojom::TabData*)>
      pinned_tab_data_changed_callback_list_;

  // List of callbacks to invoke when the pinning status for a particular tab
  // changes.
  base::RepeatingCallbackList<void(tabs::TabInterface*, bool)>
      pinning_status_changed_callback_list_;

  // Enables access to information about other sharing modes and common sharing
  // functionality.
  raw_ptr<GlicSharingManagerImpl> sharing_manager_;

  // Using a vector lets us store the pinned tabs in the order that they are
  // pinned. Searching for a pinned tab is currently linear.
  std::vector<PinnedTabEntry> pinned_tabs_;
  uint32_t max_pinned_tabs_;

  base::WeakPtrFactory<GlicPinnedTabManager> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PINNED_TAB_MANAGER_H_
