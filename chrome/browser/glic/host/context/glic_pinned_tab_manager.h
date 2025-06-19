// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PINNED_TAB_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PINNED_TAB_MANAGER_H_

#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

// Manages a collection of tabs that have been selected to be shared.
class GlicPinnedTabManager {
 public:
  GlicPinnedTabManager();
  ~GlicPinnedTabManager();

  // Registers a callback to be invoked when the pinned status of a tab changes.
  using TabPinningStatusChangedCallback =
      base::RepeatingCallback<void(tabs::TabInterface*, bool)>;
  base::CallbackListSubscription AddTabPinningStatusChangedCallback(
      TabPinningStatusChangedCallback callback);

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

  // Gets the limit on the number of pinned tabs.
  int32_t GetMaxPinnedTabs() const;

  // Gets the current number of pinned tabs.
  int32_t GetNumPinnedTabs() const;

  // Returns true if the tab is in the pinned collection.
  bool IsTabPinned(tabs::TabHandle tab_handle) const;

 private:
  class PinnedTabObserver;
  struct PinnedTabEntry {
    PinnedTabEntry(tabs::TabHandle tab_handle,
                   std::unique_ptr<PinnedTabObserver> tab_observer);
    PinnedTabEntry(PinnedTabEntry&& other);
    PinnedTabEntry& operator=(PinnedTabEntry&& other);
    ~PinnedTabEntry();
    PinnedTabEntry(const PinnedTabEntry&) = delete;
    PinnedTabEntry& operator=(const PinnedTabEntry&) = delete;
    tabs::TabHandle tab_handle;
    std::unique_ptr<PinnedTabObserver> tab_observer;
  };

  void OnTabWillClose(tabs::TabHandle tab_handles);

  // List of callbacks to invoke when the pinning status for a particular tab
  // changes.
  base::RepeatingCallbackList<void(tabs::TabInterface*, bool)>
      pinning_status_changed_callback_list_;

  // Using a vector lets us store the pinned tabs in the order that they are
  // pinned. Searching for a pinned tab is currently linear.
  std::vector<PinnedTabEntry> pinned_tabs_;
  int32_t max_pinned_tabs_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PINNED_TAB_MANAGER_H_
