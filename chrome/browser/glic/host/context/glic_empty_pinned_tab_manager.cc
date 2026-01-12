// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_empty_pinned_tab_manager.h"

namespace glic {

GlicEmptyPinnedTabManager::GlicEmptyPinnedTabManager() = default;

GlicEmptyPinnedTabManager::~GlicEmptyPinnedTabManager() = default;

base::CallbackListSubscription
GlicEmptyPinnedTabManager::AddPinnedTabsChangedCallback(
    PinnedTabsChangedCallback callback) {
  return {};
}

base::CallbackListSubscription
GlicEmptyPinnedTabManager::AddTabPinningStatusChangedCallback(
    TabPinningStatusChangedCallback callback) {
  return {};
}

base::CallbackListSubscription
GlicEmptyPinnedTabManager::AddTabPinningStatusEventCallback(
    TabPinningStatusEventCallback callback) {
  return {};
}

base::CallbackListSubscription
GlicEmptyPinnedTabManager::AddPinnedTabDataChangedCallback(
    PinnedTabDataChangedCallback callback) {
  return {};
}

bool GlicEmptyPinnedTabManager::PinTabs(
    base::span<const tabs::TabHandle> tab_handles,
    GlicPinTrigger trigger) {
  return false;
}

bool GlicEmptyPinnedTabManager::UnpinTabs(
    base::span<const tabs::TabHandle> tab_handles,
    GlicUnpinTrigger trigger) {
  return false;
}

void GlicEmptyPinnedTabManager::UnpinAllTabs(GlicUnpinTrigger trigger) {}

uint32_t GlicEmptyPinnedTabManager::SetMaxPinnedTabs(uint32_t max_pinned_tabs) {
  return 0;
}

uint32_t GlicEmptyPinnedTabManager::GetMaxPinnedTabs() const {
  return 0;
}

uint32_t GlicEmptyPinnedTabManager::GetNumPinnedTabs() const {
  return 0;
}

bool GlicEmptyPinnedTabManager::IsTabPinned(tabs::TabHandle tab_handle) const {
  return false;
}

std::optional<GlicPinnedTabUsage> GlicEmptyPinnedTabManager::GetPinnedTabUsage(
    tabs::TabHandle tab_handle) const {
  return std::nullopt;
}

std::vector<content::WebContents*> GlicEmptyPinnedTabManager::GetPinnedTabs()
    const {
  return {};
}

void GlicEmptyPinnedTabManager::SubscribeToPinCandidates(
    mojom::GetPinCandidatesOptionsPtr options,
    mojo::PendingRemote<mojom::PinCandidatesObserver> observer) {}

void GlicEmptyPinnedTabManager::OnPinnedTabContextEvent(
    tabs::TabHandle tab_handle,
    GlicPinnedTabContextEvent context_event) {}

void GlicEmptyPinnedTabManager::OnAllPinnedTabsContextEvent(
    GlicPinnedTabContextEvent context_event) {}

}  // namespace glic
