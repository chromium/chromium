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
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;
class BrowserTabStripTracker;

namespace glic {
class GlicMetrics;

enum class GlicPinnedTabContextEventType { kConversationTurnSubmitted };

struct GlicPinnedTabContextEvent {
  explicit GlicPinnedTabContextEvent(GlicPinnedTabContextEventType type);
  ~GlicPinnedTabContextEvent();

  GlicPinnedTabContextEventType type;
  base::TimeTicks timestamp;
};

// Manages a collection of tabs that have been selected to be shared.
class GlicPinnedTabManager : public TabStripModelObserver {
 public:
  explicit GlicPinnedTabManager(Profile* profile,
                                GlicInstance::UIDelegate* ui_delegate,
                                GlicMetrics* metrics);
  ~GlicPinnedTabManager() override;

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

  // Registers a callback to be invoked when a pinning status event takes place
  // for a tab. Provides richer metadata than the simple boolean callback above.
  using TabPinningStatusEventCallback =
      base::RepeatingCallback<void(tabs::TabInterface*,
                                   GlicPinningStatusEvent)>;
  base::CallbackListSubscription AddTabPinningStatusEventCallback(
      TabPinningStatusEventCallback callback);

  // Registers a callback to be invoked when the TabData for a pinned tab is
  // changed.
  using PinnedTabDataChangedCallback =
      base::RepeatingCallback<void(const TabDataChange&)>;
  base::CallbackListSubscription AddPinnedTabDataChangedCallback(
      PinnedTabDataChangedCallback callback);

  // Pins the specified tabs. If we are only able to pin `n` tabs within the
  // limit, the first `n` tabs from this collection will be pinned and we will
  // return false (to indicate that it was not fully successful). If any of the
  // tab handles correspond to a tab that either doesn't exist or is already
  // pinned, it will be skipped and we will similarly return false to indicate
  // that the function was not fully successful.
  bool PinTabs(base::span<const tabs::TabHandle> tab_handles,
               GlicPinTrigger trigger = GlicPinTrigger::kUnknown);

  // Unins the specified tabs. If any of the tab handles correspond to a tab
  // that either doesn't exist or is not pinned, it will be skipped and we will
  // similarly return false to indicate that the function was not fully
  // successful.
  bool UnpinTabs(base::span<const tabs::TabHandle> tab_handles,
                 GlicUnpinTrigger trigger = GlicUnpinTrigger::kUnknown);

  // Unpins all pinned tabs.
  void UnpinAllTabs(GlicUnpinTrigger trigger = GlicUnpinTrigger::kUnknown);

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

  // Returns the pinned tab usage for a given tab handle, if it exists.
  std::optional<GlicPinnedTabUsage> GetPinnedTabUsage(
      tabs::TabHandle tab_handle) const;

  // Fetches the current list of pinned tabs.
  std::vector<content::WebContents*> GetPinnedTabs() const;

  // Subscribes to changes in pin candidates.
  void SubscribeToPinCandidates(
      mojom::GetPinCandidatesOptionsPtr options,
      mojo::PendingRemote<mojom::PinCandidatesObserver> observer);

  // Callback for tab context events.
  void OnPinnedTabContextEvent(tabs::TabHandle tab_handle,
                               GlicPinnedTabContextEvent context_event);

  // Callback for tab context events impacting all currently pinned tabs.
  void OnAllPinnedTabsContextEvent(GlicPinnedTabContextEvent context_event);

  // Visible for testing.
  virtual bool IsBrowserValidForSharing(BrowserWindowInterface* browser_window);
  // Visible for testing.
  virtual bool IsValidForSharing(content::WebContents* web_contents);
  // Visible for testing.
  virtual bool IsGlicWindowShowing();

 private:
  class UpdateThrottler;

  // TabStripModelObserver implementation:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;
  void OnTabWillBeRemoved(content::WebContents* contents, int index) override;

  void OnPinCandidatesObserverDisconnected();

  // Sends the current list of pin candidates to the observer.
  void SendPinCandidatesUpdate();

  // Returns a vector of web contents for potential pin candidates. The vector
  // is not sorted or truncated.
  std::vector<content::WebContents*> GetUnsortedPinCandidates();

  class PinnedTabObserver;
  friend PinnedTabObserver;
  struct PinnedTabEntry {
    PinnedTabEntry(tabs::TabHandle tab_handle,
                   std::unique_ptr<PinnedTabObserver> tab_observer,
                   GlicPinnedTabUsage usage);
    ~PinnedTabEntry();
    PinnedTabEntry(PinnedTabEntry&& other);
    PinnedTabEntry& operator=(PinnedTabEntry&& other);
    PinnedTabEntry(const PinnedTabEntry&) = delete;
    PinnedTabEntry& operator=(const PinnedTabEntry&) = delete;
    tabs::TabHandle tab_handle;
    std::unique_ptr<PinnedTabObserver> tab_observer;
    GlicPinnedTabUsage usage;
  };

  // Sends an update to the web client with the full set of pinned tabs.
  void NotifyPinnedTabsChanged();

  // Returns the entry corresponding to the given tab_handle, if it exists.
  const PinnedTabEntry* GetPinnedTabEntry(tabs::TabHandle tab_handle) const;

  // Returns the pinned tab usage for a given tab_handle, if it exists.
  GlicPinnedTabUsage* GetPinnedTabUsageInternal(tabs::TabHandle tab_handle);

  // Returns true if the tab is in the pinned collection.
  bool IsTabPinned(int tab_id) const;

  // Called by friend PinnedTabObserver.
  void OnTabWillClose(tabs::TabHandle tab_handles);
  void OnTabDataChanged(tabs::TabHandle tab_handle, TabDataChange);
  void OnTabChangedOrigin(tabs::TabHandle tab_handle);

  // Callback for tab context events when we already have an entry.
  void OnPinnedTabContextEvent(GlicPinnedTabUsage& pinned_usage,
                               GlicPinnedTabContextEvent context_event);

  // List of callbacks to invoke when the collection of pinned tabs changes
  // (including changes to metadata).
  base::RepeatingCallbackList<void(const std::vector<content::WebContents*>&)>
      pinned_tabs_changed_callback_list_;

  // List of callbacks to invoke when the tab data for a pinned tab changes.
  base::RepeatingCallbackList<void(const TabDataChange&)>
      pinned_tab_data_changed_callback_list_;

  // List of callbacks to invoke when the pinning status for a particular tab
  // changes.
  base::RepeatingCallbackList<void(tabs::TabInterface*, bool)>
      pinning_status_changed_callback_list_;

  // List of callbacks to invoke when a pinning status event occurs.
  base::RepeatingCallbackList<void(tabs::TabInterface*, GlicPinningStatusEvent)>
      pinning_status_event_callback_list_;

  // Enables searching for pin_candidates.
  raw_ptr<Profile> profile_;

  raw_ptr<GlicInstance::UIDelegate> ui_delegate_;

  // Enables providing pin-related input to metrics.
  raw_ptr<GlicMetrics> metrics_;

  // Using a vector lets us store the pinned tabs in the order that they are
  // pinned. Searching for a pinned tab is currently linear.
  std::vector<PinnedTabEntry> pinned_tabs_;
  uint32_t max_pinned_tabs_;

  // The observer for pin candidate changes.
  mojo::Remote<mojom::PinCandidatesObserver> pin_candidates_observer_;

  // The options for the pin candidate observer.
  mojom::GetPinCandidatesOptionsPtr pin_candidates_options_;

  // A timer to debounce pin candidate updates.
  std::unique_ptr<UpdateThrottler> pin_candidate_updater_;

  // Tracks all the browsers for the current profile.
  std::unique_ptr<BrowserTabStripTracker> tab_strip_tracker_;

  base::WeakPtrFactory<GlicPinnedTabManager> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PINNED_TAB_MANAGER_H_
