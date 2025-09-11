// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_MANAGER_IMPL_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_MANAGER_IMPL_H_

#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/host/context/glic_focused_browser_manager.h"
#include "chrome/browser/glic/host/context/glic_focused_tab_manager.h"
#include "chrome/browser/glic/host/context/glic_pinned_tab_manager.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"

namespace glic {

class GlicMetrics;

// The error returned by the GlicSharingManager when requesting context.
struct GlicGetContextError {
  GlicGetContextFromFocusedTabError error_code;
  std::string message;
};

// The result passed from the sharing manager up to the page handler.
using GlicGetContextResult =
    base::expected<mojom::GetContextResultPtr, GlicGetContextError>;

// Implements GlicSharingManager and provides additional functionality needed
// by chrome/browser/glic. It also provides some common sharing-related
// functionality.
class GlicSharingManagerImpl : public GlicSharingManager {
 public:
  GlicSharingManagerImpl(Profile* profile,
                         GlicWindowController* window_controller,
                         GlicMetrics* metrics);
  ~GlicSharingManagerImpl() override;

  GlicSharingManagerImpl(const GlicSharingManagerImpl&) = delete;
  GlicSharingManagerImpl& operator=(const GlicSharingManagerImpl&) = delete;

  // GlicSharingManager implementation.

  using FocusedTabChangedCallback =
      base::RepeatingCallback<void(const FocusedTabData&)>;
  base::CallbackListSubscription AddFocusedTabChangedCallback(
      FocusedTabChangedCallback callback) override;

  using FocusedTabDataChangedCallback =
      base::RepeatingCallback<void(const mojom::TabData*)>;
  base::CallbackListSubscription AddFocusedTabDataChangedCallback(
      FocusedTabDataChangedCallback callback) override;
  FocusedTabData GetFocusedTabData() override;

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

  // Functionality provided for (and only used within) chrome/browser/glic.

  // Callback for changes to the focused browser (if it is potentially valid
  // for sharing).
  using FocusedBrowserChangedCallback =
      base::RepeatingCallback<void(BrowserWindowInterface*)>;
  base::CallbackListSubscription AddFocusedBrowserChangedCallback(
      FocusedBrowserChangedCallback callback);
  BrowserWindowInterface* GetFocusedBrowser() const;

  // Sets the limit on the number of pinned tabs. Returns the effective number
  // of pinned tabs. Can differ due to supporting fewer tabs than requested or
  // having more tabs currently pinned than requested.
  int32_t SetMaxPinnedTabs(uint32_t max_pinned_tabs);

  void GetContextFromTab(
      tabs::TabHandle tab_handle,
      const mojom::GetTabContextOptions& options,
      base::OnceCallback<void(GlicGetContextResult)> callback);

  void GetContextForActorFromTab(
      tabs::TabHandle tab_handle,
      const mojom::GetTabContextOptions& options,
      base::OnceCallback<void(GlicGetContextResult)> callback);

  // Fetches the current list of pinned tabs.
  std::vector<content::WebContents*> GetPinnedTabs() const override;

  // Subscribes to changes in pin candidates.
  void SubscribeToPinCandidates(
      mojom::GetPinCandidatesOptionsPtr options,
      mojo::PendingRemote<mojom::PinCandidatesObserver> observer);

  GlicFocusedBrowserManagerInterface& focused_browser_manager() {
    return focused_browser_manager_;
  }

 private:
  void GetContextFromTabImpl(
      tabs::TabInterface* tab,
      const mojom::GetTabContextOptions& options,
      base::OnceCallback<void(GlicGetContextResult)> callback);

  GlicFocusedBrowserManager focused_browser_manager_;
  GlicFocusedTabManager focused_tab_manager_;
  GlicPinnedTabManager pinned_tab_manager_;

  // The profile for which to manage sharing.
  raw_ptr<Profile> profile_;

  // Enables providing sharing-related input to metrics.
  raw_ptr<GlicMetrics> metrics_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_MANAGER_IMPL_H_
