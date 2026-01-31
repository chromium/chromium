// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PINNED_TAB_MANAGER_IMPL_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PINNED_TAB_MANAGER_IMPL_H_

#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/common/glic_tab_observer.h"
#include "chrome/browser/glic/host/context/glic_pinned_tab_manager.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace glic {
class GlicMetrics;

// Manages a collection of tabs that have been selected to be shared.
class GlicPinnedTabManagerImpl : public GlicPinnedTabManager {
 public:
  explicit GlicPinnedTabManagerImpl(Profile* profile,
                                    GlicInstance::UIDelegate* ui_delegate,
                                    GlicMetrics* metrics);
  ~GlicPinnedTabManagerImpl() override;

  // GlicPinnedTabManagerInterface implementation.
  base::CallbackListSubscription AddPinnedTabsChangedCallback(
      PinnedTabsChangedCallback callback) override;

  base::CallbackListSubscription AddTabPinningStatusChangedCallback(
      TabPinningStatusChangedCallback callback) override;

  base::CallbackListSubscription AddTabPinningStatusEventCallback(
      TabPinningStatusEventCallback callback) override;

  base::CallbackListSubscription AddPinnedTabDataChangedCallback(
      PinnedTabDataChangedCallback callback) override;

  bool PinTabs(base::span<const tabs::TabHandle> tab_handles,
               GlicPinTrigger trigger) override;

  bool UnpinTabs(base::span<const tabs::TabHandle> tab_handles,
                 GlicUnpinTrigger trigger) override;

  void UnpinAllTabs(GlicUnpinTrigger trigger) override;

  uint32_t SetMaxPinnedTabs(uint32_t max_pinned_tabs) override;

  uint32_t GetMaxPinnedTabs() const override;

  uint32_t GetNumPinnedTabs() const override;

  bool IsTabPinned(tabs::TabHandle tab_handle) const override;

  std::optional<GlicPinnedTabUsage> GetPinnedTabUsage(
      tabs::TabHandle tab_handle) const override;

  std::vector<content::WebContents*> GetPinnedTabs() const override;

  void SubscribeToPinCandidates(
      mojom::GetPinCandidatesOptionsPtr options,
      mojo::PendingRemote<mojom::PinCandidatesObserver> observer) override;

  void OnPinnedTabContextEvent(
      tabs::TabHandle tab_handle,
      GlicPinnedTabContextEvent context_event) override;

  void OnAllPinnedTabsContextEvent(
      GlicPinnedTabContextEvent context_event) override;

  // Visible for testing.
  virtual bool IsBrowserValidForSharing(BrowserWindowInterface* browser_window);
  // Visible for testing.
  virtual bool IsValidForSharing(content::WebContents* web_contents);
  // Visible for testing.
  virtual bool IsGlicWindowShowing();

 private:
  class UpdateThrottler;

  void OnTabEvent(const GlicTabEvent& event);

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

  // Tracks tab events for the current profile.
  std::unique_ptr<GlicTabObserver> tab_observer_;

  base::WeakPtrFactory<GlicPinnedTabManagerImpl> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PINNED_TAB_MANAGER_IMPL_H_
