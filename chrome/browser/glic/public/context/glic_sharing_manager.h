// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_CONTEXT_GLIC_SHARING_MANAGER_H_
#define CHROME_BROWSER_GLIC_PUBLIC_CONTEXT_GLIC_SHARING_MANAGER_H_

#include <optional>

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/host/context/glic_focused_browser_manager.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

// The error returned by the GlicSharingManagerInternal when requesting context.
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
  kNewTabDaisyChain,
  kConversationChange,
  kRestore,
  kContextMenu,
  kCandidatesToggle,
  kAtMention,
  kActuation,
  kWebClientUnknown,
  kContextualCue,
  kMaxValue = kContextualCue
};

enum class GlicUnpinTrigger {
  kUnknown,
  kInstanceDestruction,
  kConversationChangeBeforeContextShared,
  kContextMenu,
  kTabClose,
  kBackgroundTabNavigation,
  kCandidatesToggle,
  kChip,
  kActuation,
  kWebClientUnknown,
  kMaxValue = kWebClientUnknown
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

  bool IsExplicitlyPinnedByUser() const;

  // A conversation is considered unused if a conversation turn has not been
  // submitted since this tab was pinned.
  bool Unused() const;

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

// Lightweight public-facing interface for external Chrome components
// to control and observe tab pinning on a GlicInstance.
class GlicSharingManager {
 public:
  GlicSharingManager() = default;
  virtual ~GlicSharingManager() = default;
  GlicSharingManager(const GlicSharingManager&) = delete;
  GlicSharingManager& operator=(const GlicSharingManager&) = delete;

  // Registers a callback to be invoked when the pinned status of a tab changes.
  using TabPinningStatusChangedCallback =
      base::RepeatingCallback<void(tabs::TabInterface*, bool)>;
  virtual base::CallbackListSubscription AddTabPinningStatusChangedCallback(
      TabPinningStatusChangedCallback callback) = 0;

  // Pins the specified tabs. If we are only able to pin `n` tabs within the
  // limit, the first `n` tabs from this collection will be pinned and we
  // will return false (to indicate that it was not fully successful). If
  // any of the tab handles correspond to a tab that either doesn't exist or
  // is already pinned, it will be skipped and we will similarly return
  // false to indicate that the function was not fully successful.
  virtual bool PinTabs(base::span<const tabs::TabHandle> tab_handles,
                       GlicPinTrigger trigger) = 0;

  // Forwarding overload for legacy calls. Calls PinTabs with kUnknown trigger.
  bool PinTabs(base::span<const tabs::TabHandle> tab_handles) {
    return PinTabs(tab_handles, GlicPinTrigger::kUnknown);
  }

  // Unpins the specified tabs. If any of the tab handles correspond to a tab
  // that either doesn't exist or is not pinned, it will be skipped and we will
  // similarly return false to indicate that the function was not fully
  // successful.
  virtual bool UnpinTabs(base::span<const tabs::TabHandle> tab_handles,
                         GlicUnpinTrigger trigger) = 0;

  // Forwarding overload for legacy calls. Calls UnpinTabs with kUnknown
  // trigger.
  bool UnpinTabs(base::span<const tabs::TabHandle> tab_handles) {
    return UnpinTabs(tab_handles, GlicUnpinTrigger::kUnknown);
  }

  // Queries whether the given tab has been explicitly pinned.
  virtual bool IsTabPinned(tabs::TabHandle tab_handle) const = 0;
};

// Responsible for managing all shared context (focused tabs, explicitly-shared
// tabs).
class GlicSharingManagerInternal : public GlicSharingManager {
 public:
  GlicSharingManagerInternal() = default;
  ~GlicSharingManagerInternal() override = default;
  GlicSharingManagerInternal(const GlicSharingManagerInternal&) = delete;
  GlicSharingManagerInternal& operator=(const GlicSharingManagerInternal&) =
      delete;

  using GlicSharingManager::PinTabs;
  using GlicSharingManager::UnpinTabs;

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

  // GlicSharingManager override.
  base::CallbackListSubscription AddTabPinningStatusChangedCallback(
      TabPinningStatusChangedCallback callback) override = 0;

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
      base::RepeatingCallback<void(const std::vector<tabs::TabInterface*>&)>;
  virtual base::CallbackListSubscription AddPinnedTabsChangedCallback(
      PinnedTabsChangedCallback callback) = 0;

  // Registers a callback to be invoked when the TabData for a pinned tab
  // changes.
  using PinnedTabDataChangedCallback =
      base::RepeatingCallback<void(const TabDataChange&)>;
  virtual base::CallbackListSubscription AddPinnedTabDataChangedCallback(
      PinnedTabDataChangedCallback callback) = 0;

  // GlicSharingManager override.
  bool PinTabs(base::span<const tabs::TabHandle> tab_handles,
               GlicPinTrigger trigger) override = 0;

  // Overwrites the pin trigger and timestamp for an already-pinned tab.
  // This should ONLY be used when transitioning the context of a pinned tab
  // to a new conversation/instance session (such as during an in-place
  // conversation switch), without performing a full unpin and re-pin.
  virtual void SetPinTrigger(tabs::TabHandle tab_handle,
                             GlicPinTrigger trigger) = 0;

  // GlicSharingManager override.
  bool UnpinTabs(base::span<const tabs::TabHandle> tab_handles,
                 GlicUnpinTrigger trigger) override = 0;

  // Unpins all pinned tabs, if any.
  virtual void UnpinAllTabs(GlicUnpinTrigger trigger) = 0;

  // Forwarding overload for legacy calls. Calls UnpinAllTabs with kUnknown
  // trigger.
  void UnpinAllTabs() { UnpinAllTabs(GlicUnpinTrigger::kUnknown); }

  // Gets the limit on the number of pinned tabs.
  virtual int32_t GetMaxPinnedTabs() const = 0;

  // Gets the current number of pinned tabs.
  virtual int32_t GetNumPinnedTabs() const = 0;

  // Sets the limit on the number of pinned tabs. Returns the effective number
  // of pinned tabs. Can differ due to supporting fewer tabs than requested or
  // having more tabs currently pinned than requested.
  virtual int32_t SetMaxPinnedTabs(uint32_t max_pinned_tabs) = 0;

  // Fetches the current list of pinned tabs.
  virtual std::vector<tabs::TabInterface*> GetPinnedTabs() const = 0;

  // GlicSharingManager override.
  bool IsTabPinned(tabs::TabHandle tab_handle) const override = 0;

  // Queries whether the given tab is focused.
  // Note: this signal should only be used by features that care about live mode
  // where "focused" notion is still relevant. Use IsTabPinned() if the feature
  // is not compatible with live mode.
  virtual bool IsTabFocused(tabs::TabHandle tab_handle) const = 0;

  virtual std::optional<GlicPinnedTabUsage> GetPinnedTabUsage(
      tabs::TabHandle tab_handle) = 0;

  // Performs preliminary browser-side checks to determine if the context from
  // the given tab is eligible to be shared. This does not check all conditions
  // and is not the ultimate source of truth for context sharing eligibility
  // (the Glic web client is).
  virtual std::optional<GlicGetContextError>
  CheckPreliminaryContextSharingEligibility(
      tabs::TabHandle tab_handle) const = 0;

  virtual void GetContextFromTab(
      tabs::TabHandle tab_handle,
      const mojom::GetTabContextOptions& options,
      base::OnceCallback<void(GlicGetContextResult)> callback) = 0;

  virtual void GetContextForActorFromTab(
      tabs::TabHandle tab_handle,
      const mojom::GetTabContextOptions& options,
      base::OnceCallback<void(GlicGetContextResult)> callback) = 0;

  // Callback for conversation turn submission.
  virtual void OnConversationTurnSubmitted() = 0;

  virtual base::WeakPtr<GlicSharingManagerInternal> GetWeakPtr() = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_CONTEXT_GLIC_SHARING_MANAGER_H_
