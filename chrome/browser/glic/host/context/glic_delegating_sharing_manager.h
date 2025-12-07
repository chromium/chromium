// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_DELEGATING_SHARING_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_DELEGATING_SHARING_MANAGER_H_

#include "chrome/browser/glic/public/context/glic_sharing_manager.h"

namespace glic {

class GlicSharingManagerImpl;

// Sharing manager that delegates to/from another sharing manager instance.
// Provides stable external api state (e.g. callback list subscriptions), while
// also enabling hot-swapping of internal delegate.
//
// Retains its own callback list subsciptions (calls to `Add*Callback` are not
// delegated), but notify calls from current delegate are
// forwarded and all non-callback-subsciptions methods are delegated directly.
//
// This base class doesn't expose a method to set the delegate, to do so use one
// of the derived classes below instead.
class GlicDelegatingSharingManagerBase : public GlicSharingManager {
 public:
  GlicDelegatingSharingManagerBase();
  ~GlicDelegatingSharingManagerBase() override;
  GlicDelegatingSharingManagerBase(const GlicDelegatingSharingManagerBase&) =
      delete;
  GlicDelegatingSharingManagerBase& operator=(
      const GlicDelegatingSharingManagerBase&) = delete;

  // SharingManager implementation.
  using FocusedTabChangedCallback =
      base::RepeatingCallback<void(const FocusedTabData&)>;
  base::CallbackListSubscription AddFocusedTabChangedCallback(
      FocusedTabChangedCallback callback) override;
  using FocusedTabDataChangedCallback =
      base::RepeatingCallback<void(const mojom::TabData*)>;
  base::CallbackListSubscription AddFocusedTabDataChangedCallback(
      FocusedTabDataChangedCallback callback) override;
  FocusedTabData GetFocusedTabData() override;
  using FocusedBrowserChangedCallback =
      base::RepeatingCallback<void(BrowserWindowInterface*)>;
  base::CallbackListSubscription AddFocusedBrowserChangedCallback(
      FocusedBrowserChangedCallback callback) override;
  BrowserWindowInterface* GetFocusedBrowser() const override;
  using TabPinningStatusChangedCallback =
      base::RepeatingCallback<void(tabs::TabInterface*, bool)>;
  base::CallbackListSubscription AddTabPinningStatusChangedCallback(
      TabPinningStatusChangedCallback callback) override;
  using PinnedTabsChangedCallback =
      base::RepeatingCallback<void(const std::vector<content::WebContents*>&)>;
  base::CallbackListSubscription AddPinnedTabsChangedCallback(
      PinnedTabsChangedCallback callback) override;
  using PinnedTabDataChangedCallback =
      base::RepeatingCallback<void(const TabDataChange&)>;
  base::CallbackListSubscription AddPinnedTabDataChangedCallback(
      PinnedTabDataChangedCallback callback) override;
  using TabPinningStatusEventCallback =
      base::RepeatingCallback<void(tabs::TabInterface*,
                                   GlicPinningStatusEvent)>;
  base::CallbackListSubscription AddTabPinningStatusEventCallback(
      TabPinningStatusEventCallback callback) override;
  bool PinTabs(base::span<const tabs::TabHandle> tab_handles,
               GlicPinTrigger trigger) override;
  bool UnpinTabs(base::span<const tabs::TabHandle> tab_handles,
                 GlicUnpinTrigger trigger) override;
  void UnpinAllTabs(GlicUnpinTrigger trigger) override;

  std::optional<GlicPinnedTabUsage> GetPinnedTabUsage(
      tabs::TabHandle tab_handle) override;

  int32_t GetMaxPinnedTabs() const override;
  int32_t GetNumPinnedTabs() const override;
  bool IsTabPinned(tabs::TabHandle tab_handle) const override;
  int32_t SetMaxPinnedTabs(uint32_t max_pinned_tabs) override;
  std::vector<content::WebContents*> GetPinnedTabs() const override;
  void GetContextFromTab(
      tabs::TabHandle tab_handle,
      const mojom::GetTabContextOptions& options,
      base::OnceCallback<void(GlicGetContextResult)> callback) override;
  void GetContextForActorFromTab(
      tabs::TabHandle tab_handle,
      const mojom::GetTabContextOptions& options,
      base::OnceCallback<void(GlicGetContextResult)> callback) override;
  void SubscribeToPinCandidates(
      mojom::GetPinCandidatesOptionsPtr options,
      mojo::PendingRemote<mojom::PinCandidatesObserver> observer) override;
  void OnConversationTurnSubmitted() override;
  GlicFocusedBrowserManagerInterface& focused_browser_manager() override;
  base::WeakPtr<GlicSharingManager> GetWeakPtr() override;

 protected:
  // Sets the sharing manager delegate. Notifies all subscribers for all
  // callback list subscriptions.
  void SetDelegate(GlicSharingManager* sharing_manager_delegate);
  GlicSharingManager* GetDelegate();

 private:
  // Callbacks for subscribing to delegate (will be forwarded).
  void OnFocusedTabChangedCallback(const FocusedTabData& focused_tab_data);
  void OnFocusedTabDataChangedCallback(const mojom::TabData* focused_tab_data);
  void OnFocusedBrowserChangedCallback(BrowserWindowInterface* browser_window);
  void OnTabPinningStatusChangedCallback(tabs::TabInterface* tab, bool pinned);
  void OnTabPinningStatusEventCallback(tabs::TabInterface* tab,
                                       GlicPinningStatusEvent event);
  void OnPinnedTabsChangedCallback(
      const std::vector<content::WebContents*>& pinnned_tabs);
  void OnPinnedTabDataChangedCallback(const TabDataChange& tab_data_change);

  // Refreshes all internal subscriptions to point at current delegate.
  void RefreshDelegateSubscriptions();

  // Resets all internal subscriptions to empty.
  void ResetDelegateSubscriptions();

  // Forces notifications where possible.
  // TODO(b:444463509): split sharing manager interface up so it's clear which
  // notifications we actually force (i.e. what delegation is possible).
  void ForceNotify(const std::vector<content::WebContents*>& old_pinned_tabs);

  raw_ptr<GlicSharingManager> sharing_manager_delegate_;

  // Callback lists. Maintains its own callback lists to seamlessly support
  // hot-swapping delegate.
  base::RepeatingCallbackList<void(const FocusedTabData&)>
      focused_tab_changed_callback_list_;
  base::RepeatingCallbackList<void(const mojom::TabData*)>
      focused_tab_data_changed_callback_list_;
  base::RepeatingCallbackList<void(BrowserWindowInterface*)>
      focused_browser_changed_callback_list_;
  base::RepeatingCallbackList<void(tabs::TabInterface*, bool)>
      tab_pinning_status_changed_callback_list_;
  base::RepeatingCallbackList<void(tabs::TabInterface*, GlicPinningStatusEvent)>
      tab_pinning_status_event_callback_list_;
  base::RepeatingCallbackList<void(const std::vector<content::WebContents*>&)>
      pinned_tabs_changed_callback_list_;
  base::RepeatingCallbackList<void(const TabDataChange&)>
      pinned_tab_data_changed_callback_list_;

  // Callbacks. These are used to forward callback events from the delegate.
  base::CallbackListSubscription focused_tab_changed_callback_;
  base::CallbackListSubscription focused_tab_data_changed_callback_;
  base::CallbackListSubscription focused_browser_changed_callback_;
  base::CallbackListSubscription tab_pinning_status_changed_callback_;
  base::CallbackListSubscription tab_pinning_status_event_callback_;
  base::CallbackListSubscription pinned_tabs_changed_callback_;
  base::CallbackListSubscription pinned_tab_data_changed_callback_;

  base::WeakPtrFactory<GlicDelegatingSharingManagerBase> weak_ptr_factory_{
      this};
};

// A delegating sharing manager that can set any sharing manager as a delegate,
// but does not support `SubscribeToPinCandidates`.
//
// TODO(crbug.com/444463509): Once we remove single instance mode, split
// GlicSharingManager interface so we don't allow calls to
// `SubscribeToPinCandidates` (currently triggers NOTREACHED).
class GlicDelegatingSharingManager : public GlicDelegatingSharingManagerBase {
 public:
  GlicDelegatingSharingManager();
  ~GlicDelegatingSharingManager() override;

  using GlicDelegatingSharingManagerBase::SetDelegate;
};

// A delegating sharing manager that implements `SubscribeToPinCandidates` by
// treating GlicPinnedTabManager identity as an invariant among delegates.
// Behaves just like `GlicDelegatingSharingManager`, but crashes if a delegate
// is set with a different PinnedTabManager instance than the previous delegate.
//
// Useful for cases where non-pinning sharing bevhavior must change on the fly,
// but not pinning behavior (e.g. attach/detach behavior for side panel).
//
// TODO(crbug.com/444463509): Enforce invariant at compile time and move
// GlicPinnedTabManager out of WeakPtr.
class GlicStablePinningDelegatingSharingManager
    : public GlicDelegatingSharingManagerBase {
 public:
  explicit GlicStablePinningDelegatingSharingManager(
      GlicSharingManagerImpl* sharing_manager_delegate);
  ~GlicStablePinningDelegatingSharingManager() override;

  // Forwards requests to the delegate, under the assumption that the delegate's
  // GlicPinnedTabManager will never change.
  void SubscribeToPinCandidates(
      mojom::GetPinCandidatesOptionsPtr options,
      mojo::PendingRemote<mojom::PinCandidatesObserver> observer) override;

  // Changes the delegate. Crashes if the GlicPinnedTabManager instance is not
  // the same as the current delegate.
  void SetDelegate(GlicSharingManagerImpl* sharing_manager_delegate);

  // Callback for when the glic window activation state changes.
  void OnGlicWindowActivationChanged(bool active);

 private:
  // Last known state for glic window being active. We hold this so we can set
  // it on delegate swaps (delegates are not longer able to subscribe directly).
  bool glic_window_active_ = false;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_DELEGATING_SHARING_MANAGER_H_
