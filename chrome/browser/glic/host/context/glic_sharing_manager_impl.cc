// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"

#include "chrome/browser/glic/host/context/glic_tab_data.h"

namespace glic {

GlicSharingManagerImpl::GlicSharingManagerImpl(
    Profile* profile,
    GlicWindowController& window_controller,
    Host* host,
    GlicMetrics* metrics)
    : focused_tab_manager_(profile, window_controller, host, metrics) {}

GlicSharingManagerImpl::~GlicSharingManagerImpl() = default;

base::CallbackListSubscription
GlicSharingManagerImpl::AddFocusedTabChangedCallback(
    FocusedTabChangedCallback callback) {
  return focused_tab_manager_.AddFocusedTabChangedCallback(std::move(callback));
}

FocusedTabData GlicSharingManagerImpl::GetFocusedTabData() {
  return focused_tab_manager_.GetFocusedTabData();
}

base::CallbackListSubscription
GlicSharingManagerImpl::AddTabPinningStatusChangedCallback(
    TabPinningStatusChangedCallback callback) {
  return pinned_tab_manager_.AddTabPinningStatusChangedCallback(
      std::move(callback));
}

bool GlicSharingManagerImpl::PinTabs(
    base::span<const tabs::TabHandle> tab_handles) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicMultiTab));
  return pinned_tab_manager_.PinTabs(tab_handles);
}

bool GlicSharingManagerImpl::UnpinTabs(
    base::span<const tabs::TabHandle> tab_handles) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicMultiTab));
  return pinned_tab_manager_.UnpinTabs(tab_handles);
}

void GlicSharingManagerImpl::UnpinAllTabs() {
  pinned_tab_manager_.UnpinAllTabs();
}

int32_t GlicSharingManagerImpl::GetMaxPinnedTabs() const {
  return pinned_tab_manager_.GetMaxPinnedTabs();
}

int32_t GlicSharingManagerImpl::GetNumPinnedTabs() const {
  return pinned_tab_manager_.GetNumPinnedTabs();
}

bool GlicSharingManagerImpl::IsTabPinned(tabs::TabHandle tab_handle) const {
  return pinned_tab_manager_.IsTabPinned(tab_handle);
}

base::CallbackListSubscription
GlicSharingManagerImpl::AddFocusedTabDataChangedCallback(
    FocusedTabDataChangedCallback callback) {
  return focused_tab_manager_.AddFocusedTabDataChangedCallback(
      std::move(callback));
}

void GlicSharingManagerImpl::GetContextFromFocusedTab(
    const mojom::GetTabContextOptions& options,
    base::OnceCallback<void(glic::mojom::GetContextResultPtr)> callback) {
  focused_tab_manager_.GetContextFromFocusedTab(options, std::move(callback));
}

}  // namespace glic
