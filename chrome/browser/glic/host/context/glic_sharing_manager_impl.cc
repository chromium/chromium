// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"

#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/context/glic_sharing_utils.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/url_constants.h"

namespace glic {

GlicSharingManagerImpl::GlicSharingManagerImpl(
    Profile* profile,
    GlicWindowController* window_controller,
    Host* host,
    GlicMetrics* metrics)
    : focused_browser_manager_(window_controller),
      focused_tab_manager_(&focused_browser_manager_),
      pinned_tab_manager_(profile, window_controller),
      profile_(profile),
      metrics_(metrics) {}

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
  CHECK(base::FeatureList::IsEnabled(mojom::features::kGlicMultiTab));
  return pinned_tab_manager_.PinTabs(tab_handles);
}

bool GlicSharingManagerImpl::UnpinTabs(
    base::span<const tabs::TabHandle> tab_handles) {
  CHECK(base::FeatureList::IsEnabled(mojom::features::kGlicMultiTab));
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

namespace {

// Collapses notifications about either the candidate or focused browser being
// changed into a single notification. This is needed because the browser
// activation change notification is fired for both the candidate and focused
// browser, and we only want to notify the subscribers about the focused
// browser.
class FocusedBrowserChangedWatcher {
 public:
  explicit FocusedBrowserChangedWatcher(
      BrowserWindowInterface* focused_browser,
      GlicSharingManagerImpl::FocusedBrowserChangedCallback callback)
      : last_focused_browser_(focused_browser ? focused_browser->GetWeakPtr()
                                              : nullptr),
        callback_(std::move(callback)) {}

  void OnFocusedBrowserChanged(BrowserWindowInterface* candidate_browser,
                               BrowserWindowInterface* focused_browser) {
    if (last_focused_browser_.get() != focused_browser ||
        last_focused_browser_.WasInvalidated()) {
      callback_.Run(focused_browser);
    }
    last_focused_browser_ =
        focused_browser ? focused_browser->GetWeakPtr() : nullptr;
  }

 private:
  base::WeakPtr<BrowserWindowInterface> last_focused_browser_;
  const GlicSharingManagerImpl::FocusedBrowserChangedCallback callback_;
};

}  // namespace

base::CallbackListSubscription
GlicSharingManagerImpl::AddFocusedBrowserChangedCallback(
    FocusedBrowserChangedCallback callback) {
  // This callback itself keeps the `FocusedBrowserChangedWatcher` alive
  // while the subscription exists.
  return focused_browser_manager_.AddFocusedBrowserChangedCallback(
      base::BindRepeating(
          &FocusedBrowserChangedWatcher::OnFocusedBrowserChanged,
          std::make_unique<FocusedBrowserChangedWatcher>(
              focused_browser_manager_.GetFocusedBrowser(),
              std::move(callback))));
}

BrowserWindowInterface* GlicSharingManagerImpl::GetFocusedBrowser() const {
  return focused_browser_manager_.GetFocusedBrowser();
}

base::CallbackListSubscription
GlicSharingManagerImpl::AddFocusedTabDataChangedCallback(
    FocusedTabDataChangedCallback callback) {
  return focused_tab_manager_.AddFocusedTabDataChangedCallback(
      std::move(callback));
}

base::CallbackListSubscription
GlicSharingManagerImpl::AddPinnedTabsChangedCallback(
    PinnedTabsChangedCallback callback) {
  return pinned_tab_manager_.AddPinnedTabsChangedCallback(std::move(callback));
}

base::CallbackListSubscription
GlicSharingManagerImpl::AddPinnedTabDataChangedCallback(
    PinnedTabDataChangedCallback callback) {
  return pinned_tab_manager_.AddPinnedTabDataChangedCallback(
      std::move(callback));
}

int32_t GlicSharingManagerImpl::SetMaxPinnedTabs(uint32_t max_pinned_tabs) {
  return pinned_tab_manager_.SetMaxPinnedTabs(max_pinned_tabs);
}

void GlicSharingManagerImpl::GetContextFromTab(
    tabs::TabHandle tab_handle,
    const mojom::GetTabContextOptions& options,
    base::OnceCallback<void(mojom::GetContextResultPtr)> callback) {
  auto* tab = tab_handle.Get();
  if (!tab) {
    std::move(callback).Run(
        mojom::GetContextResult::NewErrorReason("tab not found"));
    return;
  }

  const bool is_pinned = pinned_tab_manager_.IsTabPinned(tab_handle);
  if (!is_pinned &&
      !profile_->GetPrefs()->GetBoolean(prefs::kGlicTabContextEnabled)) {
    std::move(callback).Run(mojom::GetContextResult::NewErrorReason(
        "permission denied: context permission not enabled"));
    return;
  }

  const bool is_focused = focused_tab_manager_.IsTabFocused(tab_handle);
  const bool is_shared = is_focused || is_pinned;
  if (!is_shared || !IsTabValidForSharing(tab->GetContents())) {
    std::move(callback).Run(
        mojom::GetContextResult::NewErrorReason("permission denied"));
    return;
  }
  if (is_focused) {
    metrics_->DidRequestContextFromFocusedTab();
  } else {
    // TODO(b/422240100): Handle metrics for pinned tabs.
  }
  FetchPageContext(tab, options, std::move(callback));
}

void GlicSharingManagerImpl::GetContextForActorFromTab(
    tabs::TabHandle tab_handle,
    const mojom::GetTabContextOptions& options,
    base::OnceCallback<void(mojom::GetContextResultPtr)> callback) {
  auto* tab = tab_handle.Get();
  if (!tab) {
    std::move(callback).Run(
        mojom::GetContextResult::NewErrorReason(std::string("tab not found")));
    return;
  }

  FetchPageContext(tab, options, std::move(callback));
}

std::vector<content::WebContents*> GlicSharingManagerImpl::GetPinnedTabs()
    const {
  return pinned_tab_manager_.GetPinnedTabs();
}

void GlicSharingManagerImpl::SubscribeToPinCandidates(
    mojom::GetPinCandidatesOptionsPtr options,
    mojo::PendingRemote<mojom::PinCandidatesObserver> observer) {
  pinned_tab_manager_.SubscribeToPinCandidates(std::move(options),
                                               std::move(observer));
}

}  // namespace glic
