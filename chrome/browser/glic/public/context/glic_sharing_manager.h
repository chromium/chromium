// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_CONTEXT_GLIC_SHARING_MANAGER_H_
#define CHROME_BROWSER_GLIC_PUBLIC_CONTEXT_GLIC_SHARING_MANAGER_H_

#include <optional>

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/host/context/glic_focused_browser_manager_interface.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

// The error returned by the GlicSharingManager when requesting context.
struct GlicGetContextError {
  GlicGetContextFromTabError error_code;
  std::string message;
};

// The result passed from the sharing manager up to the page handler.
using GlicGetContextResult =
    base::expected<mojom::GetContextResultPtr, GlicGetContextError>;

// Metadata pertaining to tab pinning.
enum class GlicPinTrigger {
  kUnknown,
  kInstanceCreation,
  kDaisyChain,
  kConversationChange,
  kRestore,
  kContextMenu,
  kCandidatesToggle,
  kAtMention,
  kActuation,
  kWebClientUnknown
};

enum class GlicUnpinTrigger {
  kUnknown,
  kInstanceDestruction,
  kConversationChangeBeforeContextShared,
  kContextMenu,
  kTabClose,
  kTabNavigationWhileInstanceFrozen,
  kCandidatesToggle,
  kChip,
  kActuation,
  kWebClientUnknown
};

struct GlicPinEvent {
  GlicPinEvent(GlicPinTrigger trigger, base::TimeTicks timestamp);

  ~GlicPinEvent();

  GlicPinTrigger trigger = GlicPinTrigger::kUnknown;
  base::TimeTicks timestamp;
};

struct GlicContextSharingStats {
  int times_requested = 0;
  int times_returned = 0;
  base::TimeTicks last_requested_timestamp;
  base::TimeTicks last_returned_timestamp;
};

struct GlicPinnedTabUsage {
  explicit GlicPinnedTabUsage(GlicPinEvent pin_event);
  GlicPinnedTabUsage(GlicPinTrigger trigger, base::TimeTicks timestamp);
  GlicPinnedTabUsage(GlicPinnedTabUsage&& other);
  GlicPinnedTabUsage(const GlicPinnedTabUsage&);
  GlicPinnedTabUsage& operator=(GlicPinnedTabUsage&& other);
  GlicPinnedTabUsage& operator=(const GlicPinnedTabUsage&);

  ~GlicPinnedTabUsage();

  GlicPinEvent pin_event;

  int times_conversation_turn_submitted_while_pinned = 0;
  int times_navigated_across_origin_while_pinned = 0;

  // Context requested/returned metadata is broken down by type below. This
  // counts overall requests (a single context fetch can request multiple types,
  // so these aren't just a sum of the breakdowns below).
  GlicContextSharingStats overall_stats;

  // Breakdowns by type. One context fetch can trigger multiple of these.
  GlicContextSharingStats apc_stats;
  GlicContextSharingStats inner_text_stats;
  GlicContextSharingStats screenshot_stats;
  // Note: PDF context is only returned when requested AND the top level
  // document is a PDF.
  GlicContextSharingStats pdf_stats;
};

struct GlicUnpinEvent {
  GlicUnpinEvent(GlicUnpinTrigger trigger,
                 GlicPinnedTabUsage usage,
                 base::TimeTicks timestamp);

  ~GlicUnpinEvent();

  GlicUnpinTrigger trigger = GlicUnpinTrigger::kUnknown;
  GlicPinnedTabUsage usage;
  base::TimeTicks timestamp;
};

// Represents a change in pinning status, used for callbacks.
using GlicPinningStatusEvent = std::variant<GlicPinEvent, GlicUnpinEvent>;

// TODO(crbug.com/461849870): Add metadata to the api below.

// Responsible for managing all shared context (focused tabs, explicitly-shared
// tabs).
class GlicSharingManager {
 public:
  GlicSharingManager() = default;
  virtual ~GlicSharingManager() = default;
  GlicSharingManager(const GlicSharingManager&) = delete;
  GlicSharingManager& operator=(const GlicSharingManager&) = delete;

  // Callback for changes to focused tab. If no tab is in focus an error reason
  // is returned indicating why and maybe a tab candidate with details as to
  // why it cannot be focused.
  using FocusedTabChangedCallback =
      base::RepeatingCallback<void(const FocusedTabData&)>;
  virtual base::CallbackListSubscription AddFocusedTabChangedCallback(
      FocusedTabChangedCallback callback) = 0;

  // Callback for changes to the tab data representation of the focused tab.
  // This includes any event that changes tab data -- e.g. favicon/title change
  // events (where the container does not change), as well as container changed
  // events.
  using FocusedTabDataChangedCallback =
      base::RepeatingCallback<void(const mojom::TabData*)>;
  virtual base::CallbackListSubscription AddFocusedTabDataChangedCallback(
      FocusedTabDataChangedCallback callback) = 0;

  // Returns the currently focused tab data or an error reason stating why one
  // was not available. This may also contain a tab candidate along with details
  // as to why it cannot be focused. Virtual for testing.
  virtual FocusedTabData GetFocusedTabData() = 0;

  // Callback for changes to the focused browser (if it is potentially valid
  // for sharing).
  using FocusedBrowserChangedCallback =
      base::RepeatingCallback<void(BrowserWindowInterface*)>;
  virtual base::CallbackListSubscription AddFocusedBrowserChangedCallback(
      FocusedBrowserChangedCallback callback) = 0;
  virtual BrowserWindowInterface* GetFocusedBrowser() const = 0;

  // TODO(b:444463509): remove direct access to underlying manager.
  virtual GlicFocusedBrowserManagerInterface& focused_browser_manager() = 0;

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

  // Registers a callback to be invoked when the collection of pinned tabs
  // changes.
  using PinnedTabsChangedCallback =
      base::RepeatingCallback<void(const std::vector<content::WebContents*>&)>;
  virtual base::CallbackListSubscription AddPinnedTabsChangedCallback(
      PinnedTabsChangedCallback callback) = 0;

  // Registers a callback to be invoked when the TabData for a pinned tab
  // changes.
  using PinnedTabDataChangedCallback =
      base::RepeatingCallback<void(const TabDataChange&)>;
  virtual base::CallbackListSubscription AddPinnedTabDataChangedCallback(
      PinnedTabDataChangedCallback callback) = 0;

  // Pins the specified tabs. If we are only able to pin `n` tabs within the
  // limit, the first `n` tabs from this collection will be pinned and we
  // will return false (to indicate that it was not fully successful). If
  // any of the tab handles correspond to a tab that either doesn't exist or
  // is already pinned, it will be skipped and we will similarly return
  // false to indicate that the function was not fully successful.
  virtual bool PinTabs(base::span<const tabs::TabHandle> tab_handles,
                       GlicPinTrigger trigger) = 0;

  // Forwarding overload for legacy calls. Calls PinTabs with kUnknown trigger.
  bool PinTabs(base::span<const tabs::TabHandle> tab_handles);

  // Unpins the specified tabs. If any of the tab handles correspond to a tab
  // that either doesn't exist or is not pinned, it will be skipped and we will
  // similarly return false to indicate that the function was not fully
  // successful.
  virtual bool UnpinTabs(base::span<const tabs::TabHandle> tab_handles,
                         GlicUnpinTrigger trigger) = 0;

  // Forwarding overload for legacy calls. Calls UnpinTabs with kUnknown
  // trigger.
  bool UnpinTabs(base::span<const tabs::TabHandle> tab_handles);

  // Unpins all pinned tabs, if any.
  virtual void UnpinAllTabs(GlicUnpinTrigger trigger) = 0;

  // Forwarding overload for legacy calls. Calls UnpinAllTabs with kUnknown
  // trigger.
  void UnpinAllTabs();

  // Gets the limit on the number of pinned tabs.
  virtual int32_t GetMaxPinnedTabs() const = 0;

  // Gets the current number of pinned tabs.
  virtual int32_t GetNumPinnedTabs() const = 0;

  // Sets the limit on the number of pinned tabs. Returns the effective number
  // of pinned tabs. Can differ due to supporting fewer tabs than requested or
  // having more tabs currently pinned than requested.
  virtual int32_t SetMaxPinnedTabs(uint32_t max_pinned_tabs) = 0;

  // Fetches the current list of pinned tabs.
  virtual std::vector<content::WebContents*> GetPinnedTabs() const = 0;

  // Queries whether the given tab has been explicitly pinned.
  virtual bool IsTabPinned(tabs::TabHandle tab_handle) const = 0;

  virtual std::optional<GlicPinnedTabUsage> GetPinnedTabUsage(
      tabs::TabHandle tab_handle) = 0;

  virtual void GetContextFromTab(
      tabs::TabHandle tab_handle,
      const mojom::GetTabContextOptions& options,
      base::OnceCallback<void(GlicGetContextResult)> callback) = 0;

  virtual void GetContextForActorFromTab(
      tabs::TabHandle tab_handle,
      const mojom::GetTabContextOptions& options,
      base::OnceCallback<void(GlicGetContextResult)> callback) = 0;

  // Subscribes to changes in pin candidates.
  virtual void SubscribeToPinCandidates(
      mojom::GetPinCandidatesOptionsPtr options,
      mojo::PendingRemote<mojom::PinCandidatesObserver> observer) = 0;

  // Callback for conversation turn submission.
  virtual void OnConversationTurnSubmitted() = 0;

  virtual base::WeakPtr<GlicSharingManager> GetWeakPtr() = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_CONTEXT_GLIC_SHARING_MANAGER_H_
