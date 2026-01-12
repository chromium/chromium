// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_EMPTY_PINNED_TAB_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_EMPTY_PINNED_TAB_MANAGER_H_

#include "chrome/browser/glic/host/context/glic_pinned_tab_manager.h"

namespace glic {

class GlicEmptyPinnedTabManager : public GlicPinnedTabManager {
 public:
  GlicEmptyPinnedTabManager();
  ~GlicEmptyPinnedTabManager() override;

  GlicEmptyPinnedTabManager(const GlicEmptyPinnedTabManager&) = delete;
  GlicEmptyPinnedTabManager& operator=(const GlicEmptyPinnedTabManager&) =
      delete;

  // GlicPinnedTabManager Implementation.
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
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_EMPTY_PINNED_TAB_MANAGER_H_
