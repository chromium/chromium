// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PINNED_TAB_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PINNED_TAB_MANAGER_H_

#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom-forward.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {
class WebContents;
}

namespace glic {

enum class GlicPinnedTabContextEventType { kConversationTurnSubmitted };

struct GlicPinnedTabContextEvent {
  explicit GlicPinnedTabContextEvent(GlicPinnedTabContextEventType type)
      : type(type), timestamp(base::TimeTicks::Now()) {}
  ~GlicPinnedTabContextEvent() = default;

  GlicPinnedTabContextEventType type;
  base::TimeTicks timestamp;
};

// Interface for managing a collection of tabs that have been selected to be
// shared (pinned).
class GlicPinnedTabManager {
 public:
  virtual ~GlicPinnedTabManager() = default;

  // Registers a callback to be invoked when the collection of pinned tabs
  // changes.
  using PinnedTabsChangedCallback =
      base::RepeatingCallback<void(const std::vector<content::WebContents*>&)>;
  virtual base::CallbackListSubscription AddPinnedTabsChangedCallback(
      PinnedTabsChangedCallback callback) = 0;

  // Registers a callback to be invoked when the pinned status of a tab changes.
  using TabPinningStatusChangedCallback =
      base::RepeatingCallback<void(tabs::TabInterface*, bool)>;
  virtual base::CallbackListSubscription AddTabPinningStatusChangedCallback(
      TabPinningStatusChangedCallback callback) = 0;

  // Registers a callback to be invoked when a pinning status event takes place
  // for a tab. Provides richer metadata than the simple boolean callback above.
  using TabPinningStatusEventCallback =
      base::RepeatingCallback<void(tabs::TabInterface*,
                                   GlicPinningStatusEvent)>;
  virtual base::CallbackListSubscription AddTabPinningStatusEventCallback(
      TabPinningStatusEventCallback callback) = 0;

  // Registers a callback to be invoked when the TabData for a pinned tab is
  // changed.
  using PinnedTabDataChangedCallback =
      base::RepeatingCallback<void(const TabDataChange&)>;
  virtual base::CallbackListSubscription AddPinnedTabDataChangedCallback(
      PinnedTabDataChangedCallback callback) = 0;

  // Pins the specified tabs. If we are only able to pin `n` tabs within the
  // limit, the first `n` tabs from this collection will be pinned and we will
  // return false (to indicate that it was not fully successful). If any of the
  // tab handles correspond to a tab that either doesn't exist or is already
  // pinned, it will be skipped and we will similarly return false to indicate
  // that the function was not fully successful.
  virtual bool PinTabs(base::span<const tabs::TabHandle> tab_handles,
                       GlicPinTrigger trigger) = 0;

  // Unins the specified tabs. If any of the tab handles correspond to a tab
  // that either doesn't exist or is not pinned, it will be skipped and we will
  // similarly return false to indicate that the function was not fully
  // successful.
  virtual bool UnpinTabs(base::span<const tabs::TabHandle> tab_handles,
                         GlicUnpinTrigger trigger) = 0;

  // Unpins all pinned tabs.
  virtual void UnpinAllTabs(GlicUnpinTrigger trigger) = 0;

  // Sets the limit on the number of pinned tabs. Returns the effective number
  // of pinned tabs. Can differ due to supporting fewer tabs than requested or
  // having more tabs currently pinned than requested.
  virtual uint32_t SetMaxPinnedTabs(uint32_t max_pinned_tabs) = 0;

  // Gets the limit on the number of pinned tabs.
  virtual uint32_t GetMaxPinnedTabs() const = 0;

  // Gets the current number of pinned tabs.
  virtual uint32_t GetNumPinnedTabs() const = 0;

  // Returns true if the tab is in the pinned collection.
  virtual bool IsTabPinned(tabs::TabHandle tab_handle) const = 0;

  // Returns the pinned tab usage for a given tab handle, if it exists.
  virtual std::optional<GlicPinnedTabUsage> GetPinnedTabUsage(
      tabs::TabHandle tab_handle) const = 0;

  // Fetches the current list of pinned tabs.
  virtual std::vector<content::WebContents*> GetPinnedTabs() const = 0;

  // Subscribes to changes in pin candidates.
  virtual void SubscribeToPinCandidates(
      mojom::GetPinCandidatesOptionsPtr options,
      mojo::PendingRemote<mojom::PinCandidatesObserver> observer) = 0;

  // Callback for tab context events.
  virtual void OnPinnedTabContextEvent(
      tabs::TabHandle tab_handle,
      GlicPinnedTabContextEvent context_event) = 0;

  // Callback for tab context events impacting all currently pinned tabs.
  virtual void OnAllPinnedTabsContextEvent(
      GlicPinnedTabContextEvent context_event) = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PINNED_TAB_MANAGER_H_
