// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_MANAGER_IMPL_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_MANAGER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/host/context/glic_focused_browser_manager.h"
#include "chrome/browser/glic/host/context/glic_focused_tab_manager.h"
#include "chrome/browser/glic/host/context/glic_pinned_tab_manager.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"

namespace glic {

class GlicMetrics;
class GlicStablePinningDelegatingSharingManager;

// Implements GlicSharingManager and provides additional functionality needed
// by chrome/browser/glic. It also provides some common sharing-related
// functionality.
class GlicSharingManagerImpl : public GlicSharingManager {
 public:
  GlicSharingManagerImpl(Profile* profile,
                         GlicWindowControllerInterface* window_controller,
                         GlicMetrics* metrics);
  GlicSharingManagerImpl(
      std::unique_ptr<GlicFocusedTabManagerInterface> focused_tab_manager,
      std::unique_ptr<GlicFocusedBrowserManagerInterface>
          focused_browser_manager,
      GlicPinnedTabManager* pinned_tab_manager,
      Profile* profile,
      GlicMetrics* metrics);
  ~GlicSharingManagerImpl() override;

  GlicSharingManagerImpl(const GlicSharingManagerImpl&) = delete;
  GlicSharingManagerImpl& operator=(const GlicSharingManagerImpl&) = delete;

  // Grants special access to internals for enforcing invariants,
  // without exposing generally.
  friend class GlicStablePinningDelegatingSharingManager;

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

 private:
  void GetContextFromTabImpl(
      tabs::TabInterface* tab,
      const mojom::GetTabContextOptions& options,
      base::OnceCallback<void(GlicGetContextResult)> callback);

  GlicPinnedTabManager* pinned_tab_manager() const;

  std::unique_ptr<GlicFocusedBrowserManagerInterface> focused_browser_manager_;
  std::unique_ptr<GlicFocusedTabManagerInterface> focused_tab_manager_;
  std::variant<std::unique_ptr<GlicPinnedTabManager>,
               raw_ptr<GlicPinnedTabManager>>
      pinned_tab_manager_;

  // The profile for which to manage sharing.
  raw_ptr<Profile> profile_;

  // Enables providing sharing-related input to metrics.
  raw_ptr<GlicMetrics> metrics_;

  base::WeakPtrFactory<GlicSharingManagerImpl> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_MANAGER_IMPL_H_
