// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_delegating_sharing_manager.h"

#include "base/callback_list.h"

namespace glic {

GlicDelegatingSharingManager::~GlicDelegatingSharingManager() = default;

base::CallbackListSubscription
GlicDelegatingSharingManager::AddFocusedTabChangedCallback(
    FocusedTabChangedCallback callback) {
  return focused_tab_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicDelegatingSharingManager::AddTabPinningStatusChangedCallback(
    TabPinningStatusChangedCallback callback) {
  return tab_pinning_status_changed_callback_list_.Add(std::move(callback));
}

FocusedTabData GlicDelegatingSharingManager::GetFocusedTabData() {
  return sharing_manager_delegate_
             ? sharing_manager_delegate_->GetFocusedTabData()
             : FocusedTabData(std::string("no focusable tab"), nullptr);
}

bool GlicDelegatingSharingManager::PinTabs(
    base::span<const tabs::TabHandle> tab_handles) {
  return sharing_manager_delegate_
             ? sharing_manager_delegate_->PinTabs(tab_handles)
             : false;
}

bool GlicDelegatingSharingManager::UnpinTabs(
    base::span<const tabs::TabHandle> tab_handles) {
  return sharing_manager_delegate_
             ? sharing_manager_delegate_->UnpinTabs(tab_handles)
             : false;
}

void GlicDelegatingSharingManager::UnpinAllTabs() {
  if (sharing_manager_delegate_) {
    sharing_manager_delegate_->UnpinAllTabs();
  }
}

int32_t GlicDelegatingSharingManager::GetMaxPinnedTabs() const {
  return sharing_manager_delegate_
             ? sharing_manager_delegate_->GetMaxPinnedTabs()
             : 0;
}

int32_t GlicDelegatingSharingManager::GetNumPinnedTabs() const {
  return sharing_manager_delegate_
             ? sharing_manager_delegate_->GetNumPinnedTabs()
             : 0;
}

bool GlicDelegatingSharingManager::IsTabPinned(
    tabs::TabHandle tab_handle) const {
  return sharing_manager_delegate_
             ? sharing_manager_delegate_->IsTabPinned(tab_handle)
             : false;
}

base::CallbackListSubscription
GlicDelegatingSharingManager::AddFocusedBrowserChangedCallback(
    FocusedBrowserChangedCallback callback) {
  return focused_browser_changed_callback_list_.Add(std::move(callback));
}

BrowserWindowInterface* GlicDelegatingSharingManager::GetFocusedBrowser()
    const {
  return sharing_manager_delegate_
             ? sharing_manager_delegate_->GetFocusedBrowser()
             : nullptr;
}

GlicFocusedBrowserManagerInterface&
GlicDelegatingSharingManager::focused_browser_manager() {
  // Exposing this directly would break delegation strategy.
  // TODO(b:444463509): remove direct manager access from the interface.
  NOTREACHED();
}

base::CallbackListSubscription
GlicDelegatingSharingManager::AddFocusedTabDataChangedCallback(
    FocusedTabDataChangedCallback callback) {
  return focused_tab_data_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicDelegatingSharingManager::AddPinnedTabsChangedCallback(
    PinnedTabsChangedCallback callback) {
  return pinned_tabs_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicDelegatingSharingManager::AddPinnedTabDataChangedCallback(
    PinnedTabDataChangedCallback callback) {
  return pinned_tab_data_changed_callback_list_.Add(std::move(callback));
}

int32_t GlicDelegatingSharingManager::SetMaxPinnedTabs(
    uint32_t max_pinned_tabs) {
  return sharing_manager_delegate_
             ? sharing_manager_delegate_->SetMaxPinnedTabs(max_pinned_tabs)
             : 0;
}

void GlicDelegatingSharingManager::GetContextFromTab(
    tabs::TabHandle tab_handle,
    const mojom::GetTabContextOptions& options,
    base::OnceCallback<void(GlicGetContextResult)> callback) {
  if (!sharing_manager_delegate_) {
    std::move(callback).Run(base::unexpected(GlicGetContextError{
        GlicGetContextFromFocusedTabError::kPageContextNotEligible,
        "tab not eligible"}));
    return;
  }

  sharing_manager_delegate_->GetContextFromTab(tab_handle, options,
                                               std::move(callback));
}

void GlicDelegatingSharingManager::GetContextForActorFromTab(
    tabs::TabHandle tab_handle,
    const mojom::GetTabContextOptions& options,
    base::OnceCallback<void(GlicGetContextResult)> callback) {
  if (!sharing_manager_delegate_) {
    std::move(callback).Run(base::unexpected(GlicGetContextError{
        GlicGetContextFromFocusedTabError::kPageContextNotEligible,
        "tab not eligible"}));
    return;
  }

  sharing_manager_delegate_->GetContextForActorFromTab(tab_handle, options,
                                                       std::move(callback));
}

std::vector<content::WebContents*> GlicDelegatingSharingManager::GetPinnedTabs()
    const {
  return sharing_manager_delegate_ ? sharing_manager_delegate_->GetPinnedTabs()
                                   : std::vector<content::WebContents*>{};
}

void GlicDelegatingSharingManager::SubscribeToPinCandidates(
    mojom::GetPinCandidatesOptionsPtr options,
    mojo::PendingRemote<mojom::PinCandidatesObserver> observer) {
  // TODO(b:444463509): support dynamic subscription streaming for handling
  // per-instance sharing manager delegation (e.g. attach/detach).
  NOTREACHED();
}

void GlicDelegatingSharingManager::SetDelegate(
    base::WeakPtr<GlicSharingManager> sharing_manager_delegate) {
  // TODO(b:444463509): Wire-up callback list subscriptions and trigger upstream
  // callbacks.
  sharing_manager_delegate_ = sharing_manager_delegate;
}

}  // namespace glic
