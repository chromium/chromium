// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_MANAGER_IMPL_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_MANAGER_IMPL_H_

#include "chrome/browser/glic/host/context/glic_focused_tab_manager.h"
#include "chrome/browser/glic/host/context/glic_pinned_tab_manager.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"

namespace glic {

class GlicMetrics;

// Implements GlicSharingManager and provides additional functionality needed
// by chrome/browser/glic. It also provides some common sharing-related
// functionality.
class GlicSharingManagerImpl : public GlicSharingManager {
 public:
  GlicSharingManagerImpl(Profile* profile,
                         GlicWindowController* window_controller,
                         Host* host,
                         GlicMetrics* metrics);
  ~GlicSharingManagerImpl() override;

  GlicSharingManagerImpl(const GlicSharingManagerImpl&) = delete;
  GlicSharingManagerImpl& operator=(const GlicSharingManagerImpl&) = delete;

  // GlicSharingManager implementation.

  using FocusedTabChangedCallback =
      base::RepeatingCallback<void(const FocusedTabData&)>;
  base::CallbackListSubscription AddFocusedTabChangedCallback(
      FocusedTabChangedCallback callback) override;

  FocusedTabData GetFocusedTabData() override;

  using TabPinningStatusChangedCallback =
      base::RepeatingCallback<void(tabs::TabInterface*, bool)>;
  base::CallbackListSubscription AddTabPinningStatusChangedCallback(
      TabPinningStatusChangedCallback callback) override;

  bool PinTabs(base::span<const tabs::TabHandle> tab_handles) override;

  bool UnpinTabs(base::span<const tabs::TabHandle> tab_handles) override;

  void UnpinAllTabs() override;

  int32_t GetMaxPinnedTabs() const override;

  int32_t GetNumPinnedTabs() const override;

  bool IsTabPinned(tabs::TabHandle tab_handle) const override;

  // Functionality provided for (and only used within) chrome/browser/glic.

  // Callback for changes to the tab data representation of the focused tab.
  // This includes any event that changes tab data -- e.g. favicon/title change
  // events (where the container does not change), as well as container changed
  // events.
  using FocusedTabDataChangedCallback =
      base::RepeatingCallback<void(const glic::mojom::TabData*)>;
  base::CallbackListSubscription AddFocusedTabDataChangedCallback(
      FocusedTabDataChangedCallback callback);

  // Registers a callback to be invoked when the collection of pinned tabs
  // changes.
  using PinnedTabsChangedCallback =
      base::RepeatingCallback<void(const std::vector<content::WebContents*>&)>;
  base::CallbackListSubscription AddPinnedTabsChangedCallback(
      PinnedTabsChangedCallback callback);

  // Registers a callback to be invoked when the TabData for a pinned tab
  // changes.
  using PinnedTabDataChangedCallback =
      base::RepeatingCallback<void(const glic::mojom::TabData*)>;
  base::CallbackListSubscription AddPinnedTabDataChangedCallback(
      PinnedTabDataChangedCallback callback);

  // Sets the limit on the number of pinned tabs. Returns the effective number
  // of pinned tabs. Can differ due to supporting fewer tabs than requested or
  // having more tabs currently pinned than requested.
  int32_t SetMaxPinnedTabs(uint32_t max_pinned_tabs);

  void GetContextFromTab(
      tabs::TabHandle tab_handle,
      const mojom::GetTabContextOptions& options,
      base::OnceCallback<void(glic::mojom::GetContextResultPtr)> callback);

  // True if the immutable attributes of `browser` are valid for Glic focus.
  // or pinning. Invalid browsers are never observed.
  bool IsBrowserValidForSharing(BrowserWindowInterface* browser_interface);

  // True if the given contents are a candidate for sharing. Performs a number
  // of checks, but sharing may still fail for other reasons.
  bool IsValidCandidateForSharing(content::WebContents* contents);

  // Fetches the current list of pinned tabs.
  std::vector<content::WebContents*> GetPinnedTabs() const;

 private:
  GlicFocusedTabManager focused_tab_manager_;
  GlicPinnedTabManager pinned_tab_manager_;

  // The profile for which to manage sharing.
  raw_ptr<Profile> profile_;

  // The Glic window controller.
  raw_ref<GlicWindowController> window_controller_;

  base::flat_set<GURL> url_allow_list_;

  // Enables providing sharing-related-related input to metrics.
  raw_ptr<GlicMetrics> metrics_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_MANAGER_IMPL_H_
