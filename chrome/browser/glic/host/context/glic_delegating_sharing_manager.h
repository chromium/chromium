// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_DELEGATING_SHARING_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_DELEGATING_SHARING_MANAGER_H_

#include "chrome/browser/glic/public/context/glic_sharing_manager.h"

namespace glic {

// Sharing manager that delegates to/from another sharing manager instance.
// Provides stable external api state (e.g. callback list subscriptions), while
// also enabling hot-swapping of internal delegate.
//
// Retains its own callback list subsciptions (calls to `Add*Callback` are not
// delegated), but notify calls from current delegate are
// forwarded and all non-callback-subsciptions methods are delegated directly.
class GlicDelegatingSharingManager : public GlicSharingManager {
 public:
  GlicDelegatingSharingManager();
  ~GlicDelegatingSharingManager() override;
  GlicDelegatingSharingManager(const GlicDelegatingSharingManager&) = delete;
  GlicDelegatingSharingManager& operator=(const GlicDelegatingSharingManager&) =
      delete;

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
  bool PinTabs(base::span<const tabs::TabHandle> tab_handles) override;
  bool UnpinTabs(base::span<const tabs::TabHandle> tab_handles) override;
  void UnpinAllTabs() override;
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
  GlicFocusedBrowserManagerInterface& focused_browser_manager() override;

  // Sets the sharing manager delegate. Notifies all subscribers for all
  // callback list subscriptions.
  void SetDelegate(base::WeakPtr<GlicSharingManager> sharing_manager_delegate);

 private:
  base::WeakPtr<GlicSharingManager> sharing_manager_delegate_;

  // Callbacks for subscribing to delegate (will be forwarded).
  void OnFocusedTabChangedCallback(const FocusedTabData& focused_tab_data);
  void OnFocusedTabDataChangedCallback(const mojom::TabData* focused_tab_data);
  void OnFocusedBrowserChangedCallback(BrowserWindowInterface* browser_window);
  void OnTabPinningStatusChangedCallback(tabs::TabInterface* tab, bool pinned);
  void OnPinnedTabsChangedCallback(
      const std::vector<content::WebContents*>& pinnned_tabs);
  void OnPinnedTabDataChangedCallback(const TabDataChange& tab_data_change);

  // Refreshes all internal subscriptions to point at current delegate.
  void RefreshDelegateSubscriptions();

  // Resets all internal subscriptions to empty.
  void ResetDelegateSubscriptions();

  // Forces notifications for all callbacks with associated getters.
  void ForceNotify();

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
  base::RepeatingCallbackList<void(const std::vector<content::WebContents*>&)>
      pinned_tabs_changed_callback_list_;
  base::RepeatingCallbackList<void(const TabDataChange&)>
      pinned_tab_data_changed_callback_list_;

  // Callbacks. These are used to forward callback events from the delegate.
  base::CallbackListSubscription focused_tab_changed_callback_;
  base::CallbackListSubscription focused_tab_data_changed_callback_;
  base::CallbackListSubscription focused_browser_changed_callback_;
  base::CallbackListSubscription tab_pinning_status_changed_callback_;
  base::CallbackListSubscription pinned_tabs_changed_callback_;
  base::CallbackListSubscription pinned_tab_data_changed_callback_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_DELEGATING_SHARING_MANAGER_H_
