// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_delegating_sharing_manager.h"

#include "base/callback_list.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/context/glic_sharing_utils.h"

namespace glic {

GlicDelegatingSharingManagerBase::GlicDelegatingSharingManagerBase() = default;
GlicDelegatingSharingManagerBase::~GlicDelegatingSharingManagerBase() = default;

base::CallbackListSubscription
GlicDelegatingSharingManagerBase::AddFocusedTabChangedCallback(
    FocusedTabChangedCallback callback) {
  return focused_tab_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicDelegatingSharingManagerBase::AddTabPinningStatusChangedCallback(
    TabPinningStatusChangedCallback callback) {
  return tab_pinning_status_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicDelegatingSharingManagerBase::AddTabPinningStatusEventCallback(
    TabPinningStatusEventCallback callback) {
  return tab_pinning_status_event_callback_list_.Add(std::move(callback));
}

FocusedTabData GlicDelegatingSharingManagerBase::GetFocusedTabData() {
  return sharing_manager_delegate_
             ? sharing_manager_delegate_->GetFocusedTabData()
             : FocusedTabData(std::string("no focusable tab"), nullptr);
}

bool GlicDelegatingSharingManagerBase::PinTabs(
    base::span<const tabs::TabHandle> tab_handles,
    GlicPinTrigger trigger) {
  return sharing_manager_delegate_
             ? sharing_manager_delegate_->PinTabs(tab_handles, trigger)
             : false;
}

bool GlicDelegatingSharingManagerBase::UnpinTabs(
    base::span<const tabs::TabHandle> tab_handles,
    GlicUnpinTrigger trigger) {
  return sharing_manager_delegate_
             ? sharing_manager_delegate_->UnpinTabs(tab_handles, trigger)
             : false;
}

void GlicDelegatingSharingManagerBase::UnpinAllTabs(GlicUnpinTrigger trigger) {
  if (sharing_manager_delegate_) {
    sharing_manager_delegate_->UnpinAllTabs(trigger);
  }
}

std::optional<GlicPinnedTabUsage>
GlicDelegatingSharingManagerBase::GetPinnedTabUsage(
    tabs::TabHandle tab_handle) {
  return sharing_manager_delegate_
             ? sharing_manager_delegate_->GetPinnedTabUsage(tab_handle)
             : std::nullopt;
}

int32_t GlicDelegatingSharingManagerBase::GetMaxPinnedTabs() const {
  return sharing_manager_delegate_
             ? sharing_manager_delegate_->GetMaxPinnedTabs()
             : 0;
}

int32_t GlicDelegatingSharingManagerBase::GetNumPinnedTabs() const {
  return sharing_manager_delegate_
             ? sharing_manager_delegate_->GetNumPinnedTabs()
             : 0;
}

bool GlicDelegatingSharingManagerBase::IsTabPinned(
    tabs::TabHandle tab_handle) const {
  return sharing_manager_delegate_
             ? sharing_manager_delegate_->IsTabPinned(tab_handle)
             : false;
}

base::CallbackListSubscription
GlicDelegatingSharingManagerBase::AddFocusedBrowserChangedCallback(
    FocusedBrowserChangedCallback callback) {
  return focused_browser_changed_callback_list_.Add(std::move(callback));
}

BrowserWindowInterface* GlicDelegatingSharingManagerBase::GetFocusedBrowser()
    const {
  return sharing_manager_delegate_
             ? sharing_manager_delegate_->GetFocusedBrowser()
             : nullptr;
}

GlicFocusedBrowserManagerInterface&
GlicDelegatingSharingManagerBase::focused_browser_manager() {
  // Exposing this directly would break delegation strategy.
  // TODO(b:444463509): remove direct manager access from the interface.
  NOTREACHED();
}

base::CallbackListSubscription
GlicDelegatingSharingManagerBase::AddFocusedTabDataChangedCallback(
    FocusedTabDataChangedCallback callback) {
  return focused_tab_data_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicDelegatingSharingManagerBase::AddPinnedTabsChangedCallback(
    PinnedTabsChangedCallback callback) {
  return pinned_tabs_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicDelegatingSharingManagerBase::AddPinnedTabDataChangedCallback(
    PinnedTabDataChangedCallback callback) {
  return pinned_tab_data_changed_callback_list_.Add(std::move(callback));
}

int32_t GlicDelegatingSharingManagerBase::SetMaxPinnedTabs(
    uint32_t max_pinned_tabs) {
  return sharing_manager_delegate_
             ? sharing_manager_delegate_->SetMaxPinnedTabs(max_pinned_tabs)
             : 0;
}

void GlicDelegatingSharingManagerBase::GetContextFromTab(
    tabs::TabHandle tab_handle,
    const mojom::GetTabContextOptions& options,
    base::OnceCallback<void(GlicGetContextResult)> callback) {
  if (!sharing_manager_delegate_) {
    std::move(callback).Run(base::unexpected(
        GlicGetContextError{GlicGetContextFromTabError::kPageContextNotEligible,
                            "tab not eligible"}));
    return;
  }

  sharing_manager_delegate_->GetContextFromTab(tab_handle, options,
                                               std::move(callback));
}

void GlicDelegatingSharingManagerBase::GetContextForActorFromTab(
    tabs::TabHandle tab_handle,
    const mojom::GetTabContextOptions& options,
    base::OnceCallback<void(GlicGetContextResult)> callback) {
  if (!sharing_manager_delegate_) {
    std::move(callback).Run(base::unexpected(
        GlicGetContextError{GlicGetContextFromTabError::kPageContextNotEligible,
                            "tab not eligible"}));
    return;
  }

  sharing_manager_delegate_->GetContextForActorFromTab(tab_handle, options,
                                                       std::move(callback));
}

std::vector<content::WebContents*>
GlicDelegatingSharingManagerBase::GetPinnedTabs() const {
  return sharing_manager_delegate_ ? sharing_manager_delegate_->GetPinnedTabs()
                                   : std::vector<content::WebContents*>{};
}

void GlicDelegatingSharingManagerBase::SubscribeToPinCandidates(
    mojom::GetPinCandidatesOptionsPtr options,
    mojo::PendingRemote<mojom::PinCandidatesObserver> observer) {
  // TODO(b:444463509): support dynamic subscription streaming for handling
  // per-instance sharing manager delegation (e.g. attach/detach).
  NOTREACHED();
}

void GlicDelegatingSharingManagerBase::OnConversationTurnSubmitted() {
  if (sharing_manager_delegate_) {
    sharing_manager_delegate_->OnConversationTurnSubmitted();
  }
}

base::WeakPtr<GlicSharingManager>
GlicDelegatingSharingManagerBase::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void GlicDelegatingSharingManagerBase::SetDelegate(
    GlicSharingManager* sharing_manager_delegate) {
  // Do nothing if the delegate hasn't changed.
  if (sharing_manager_delegate == sharing_manager_delegate_) {
    return;
  }
  // Grab currently pinned tabs before swapping delegate so we can fire pinned
  // status updates on them.
  auto old_pinned_tabs = GetPinnedTabs();
  sharing_manager_delegate_ = sharing_manager_delegate;
  RefreshDelegateSubscriptions();
  ForceNotify(old_pinned_tabs);
}

GlicSharingManager* GlicDelegatingSharingManagerBase::GetDelegate() {
  return sharing_manager_delegate_;
}

void GlicDelegatingSharingManagerBase::OnFocusedTabChangedCallback(
    const FocusedTabData& focused_tab_data) {
  focused_tab_changed_callback_list_.Notify(focused_tab_data);
}

void GlicDelegatingSharingManagerBase::OnFocusedTabDataChangedCallback(
    const mojom::TabData* focused_tab_data) {
  focused_tab_data_changed_callback_list_.Notify(focused_tab_data);
}

void GlicDelegatingSharingManagerBase::OnFocusedBrowserChangedCallback(
    BrowserWindowInterface* browser_window) {
  focused_browser_changed_callback_list_.Notify(browser_window);
}

void GlicDelegatingSharingManagerBase::OnTabPinningStatusChangedCallback(
    tabs::TabInterface* tab,
    bool pinned) {
  tab_pinning_status_changed_callback_list_.Notify(tab, pinned);
}

void GlicDelegatingSharingManagerBase::OnTabPinningStatusEventCallback(
    tabs::TabInterface* tab,
    GlicPinningStatusEvent event) {
  tab_pinning_status_event_callback_list_.Notify(tab, event);
}

void GlicDelegatingSharingManagerBase::OnPinnedTabsChangedCallback(
    const std::vector<content::WebContents*>& pinned_tabs) {
  pinned_tabs_changed_callback_list_.Notify(pinned_tabs);
}

void GlicDelegatingSharingManagerBase::OnPinnedTabDataChangedCallback(
    const TabDataChange& tab_data_change) {
  pinned_tab_data_changed_callback_list_.Notify(tab_data_change);
}

void GlicDelegatingSharingManagerBase::ResetDelegateSubscriptions() {
  focused_tab_changed_callback_ = {};
  focused_tab_data_changed_callback_ = {};
  focused_browser_changed_callback_ = {};
  tab_pinning_status_changed_callback_ = {};
  pinned_tabs_changed_callback_ = {};
  pinned_tab_data_changed_callback_ = {};
}

void GlicDelegatingSharingManagerBase::RefreshDelegateSubscriptions() {
  if (!sharing_manager_delegate_) {
    ResetDelegateSubscriptions();
    return;
  }

  focused_tab_changed_callback_ =
      sharing_manager_delegate_->AddFocusedTabChangedCallback(
          base::BindRepeating(
              &GlicDelegatingSharingManagerBase::OnFocusedTabChangedCallback,
              base::Unretained(this)));
  focused_tab_data_changed_callback_ =
      sharing_manager_delegate_->AddFocusedTabDataChangedCallback(
          base::BindRepeating(&GlicDelegatingSharingManagerBase::
                                  OnFocusedTabDataChangedCallback,
                              base::Unretained(this)));
  focused_browser_changed_callback_ =
      sharing_manager_delegate_->AddFocusedBrowserChangedCallback(
          base::BindRepeating(&GlicDelegatingSharingManagerBase::
                                  OnFocusedBrowserChangedCallback,
                              base::Unretained(this)));
  tab_pinning_status_changed_callback_ =
      sharing_manager_delegate_->AddTabPinningStatusChangedCallback(
          base::BindRepeating(&GlicDelegatingSharingManagerBase::
                                  OnTabPinningStatusChangedCallback,
                              base::Unretained(this)));
  tab_pinning_status_event_callback_ =
      sharing_manager_delegate_->AddTabPinningStatusEventCallback(
          base::BindRepeating(&GlicDelegatingSharingManagerBase::
                                  OnTabPinningStatusEventCallback,
                              base::Unretained(this)));
  pinned_tabs_changed_callback_ =
      sharing_manager_delegate_->AddPinnedTabsChangedCallback(
          base::BindRepeating(
              &GlicDelegatingSharingManagerBase::OnPinnedTabsChangedCallback,
              base::Unretained(this)));
  pinned_tab_data_changed_callback_ =
      sharing_manager_delegate_->AddPinnedTabDataChangedCallback(
          base::BindRepeating(
              &GlicDelegatingSharingManagerBase::OnPinnedTabDataChangedCallback,
              base::Unretained(this)));
}

void GlicDelegatingSharingManagerBase::ForceNotify(
    const std::vector<content::WebContents*>& old_pinned_tabs) {
  for (auto* tab : old_pinned_tabs) {
    tab_pinning_status_changed_callback_list_.Notify(
        tabs::TabInterface::GetFromContents(tab), false);
    // TODO(crbug.com/461849870): once metadata getters are implemented,
    // consider caching and returning a meaningful event here.
    tab_pinning_status_event_callback_list_.Notify(
        tabs::TabInterface::GetFromContents(tab), GetEmptyUnpinEvent());
  }

  for (auto* tab : GetPinnedTabs()) {
    tab_pinning_status_changed_callback_list_.Notify(
        tabs::TabInterface::GetFromContents(tab), true);
    // TODO(crbug.com/461849870): once metadata getters are implemented,
    // consider returning a meaningful event here.
    tab_pinning_status_event_callback_list_.Notify(
        tabs::TabInterface::GetFromContents(tab), GetEmptyPinEvent());
  }

  // Note: in the case where delegate is now null, we still want to fire these
  // (with empty arguments).
  focused_tab_changed_callback_list_.Notify(GetFocusedTabData());
  focused_browser_changed_callback_list_.Notify(GetFocusedBrowser());
  pinned_tabs_changed_callback_list_.Notify(GetPinnedTabs());
}

GlicDelegatingSharingManager::GlicDelegatingSharingManager() = default;
GlicDelegatingSharingManager::~GlicDelegatingSharingManager() = default;

GlicStablePinningDelegatingSharingManager::
    GlicStablePinningDelegatingSharingManager(
        GlicSharingManagerImpl* sharing_manager_delegate) {
  CHECK(sharing_manager_delegate &&
        sharing_manager_delegate->pinned_tab_manager());
  GlicDelegatingSharingManagerBase::SetDelegate(sharing_manager_delegate);
}

GlicStablePinningDelegatingSharingManager::
    ~GlicStablePinningDelegatingSharingManager() = default;

void GlicStablePinningDelegatingSharingManager::SetDelegate(
    GlicSharingManagerImpl* sharing_manager_delegate) {
  GlicSharingManagerImpl* old_delegate =
      static_cast<GlicSharingManagerImpl*>(GetDelegate());
  CHECK(sharing_manager_delegate && old_delegate);
  CHECK(sharing_manager_delegate->pinned_tab_manager() &&
        sharing_manager_delegate->pinned_tab_manager() ==
            old_delegate->pinned_tab_manager());
  GlicDelegatingSharingManagerBase::SetDelegate(sharing_manager_delegate);

  // Make sure Glic window activation state is current since multi-instance
  // doesn't allow the focused browser manager to handle its own subscription.
  GlicSharingManagerImpl* delegate =
      static_cast<GlicSharingManagerImpl*>(GetDelegate());
  delegate->focused_browser_manager_->OnGlicWindowActivationChanged(
      glic_window_active_);
}

void GlicStablePinningDelegatingSharingManager::SubscribeToPinCandidates(
    mojom::GetPinCandidatesOptionsPtr options,
    mojo::PendingRemote<mojom::PinCandidatesObserver> observer) {
  if (GetDelegate()) {
    GetDelegate()->SubscribeToPinCandidates(std::move(options),
                                            std::move(observer));
  }
}

void GlicStablePinningDelegatingSharingManager::OnGlicWindowActivationChanged(
    bool active) {
  glic_window_active_ = active;
  GlicSharingManagerImpl* delegate =
      static_cast<GlicSharingManagerImpl*>(GetDelegate());
  delegate->focused_browser_manager_->OnGlicWindowActivationChanged(active);
}

}  // namespace glic
