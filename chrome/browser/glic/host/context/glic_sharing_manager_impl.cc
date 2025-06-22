// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"

#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
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
    : focused_tab_manager_(window_controller, this),
      pinned_tab_manager_(this),
      profile_(profile),
      window_controller_(*window_controller),
      // We allow allow blank pages to avoid flicker during transitions.
      url_allow_list_({GURL(), GURL("about:blank"),
                       GURL(chrome::kChromeUINewTabPageThirdPartyURL),
                       GURL(chrome::kChromeUINewTabPageURL),
                       GURL(chrome::kChromeUINewTabURL),
                       GURL(chrome::kChromeUIWhatsNewURL)}),
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
  CHECK(base::FeatureList::IsEnabled(glic::mojom::features::kGlicMultiTab));
  return pinned_tab_manager_.PinTabs(tab_handles);
}

bool GlicSharingManagerImpl::UnpinTabs(
    base::span<const tabs::TabHandle> tab_handles) {
  CHECK(base::FeatureList::IsEnabled(glic::mojom::features::kGlicMultiTab));
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
    base::OnceCallback<void(glic::mojom::GetContextResultPtr)> callback) {
  if (!profile_->GetPrefs()->GetBoolean(prefs::kGlicTabContextEnabled) ||
      !window_controller_->IsShowing()) {
    std::move(callback).Run(mojom::GetContextResult::NewErrorReason(
        std::string("permission denied")));
    return;
  }

  auto* tab = tab_handle.Get();
  if (!tab) {
    std::move(callback).Run(
        mojom::GetContextResult::NewErrorReason(std::string("tab not found")));
    return;
  }

  const bool is_focused = focused_tab_manager_.IsTabFocused(tab_handle);
  const bool is_pinned = pinned_tab_manager_.IsTabPinned(tab_handle);
  const bool is_shared = is_focused || is_pinned;
  if (!is_shared || !IsValidCandidateForSharing(tab->GetContents())) {
    std::move(callback).Run(mojom::GetContextResult::NewErrorReason(
        std::string("permission denied")));
    return;
  }
  if (is_focused) {
    metrics_->DidRequestContextFromFocusedTab();
  } else {
    // TODO(b/422240100): Handle metrics for pinned tabs.
  }
  FetchPageContext(tab, options,
                   /*include_actionable_data=*/false, std::move(callback));
}

bool GlicSharingManagerImpl::IsBrowserValidForSharing(
    BrowserWindowInterface* browser_interface) {
  if (!browser_interface) {
    return false;
  }

  if (browser_interface->GetProfile() != profile_) {
    return false;
  }

  if (browser_interface->GetProfile()->IsOffTheRecord()) {
    return false;
  }

  return true;
}

bool GlicSharingManagerImpl::IsValidCandidateForSharing(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }
  auto url = web_contents->GetLastCommittedURL();
  return url.SchemeIsHTTPOrHTTPS() || url.SchemeIsFile() ||
         url_allow_list_.contains(url);
}

std::vector<content::WebContents*> GlicSharingManagerImpl::GetPinnedTabs()
    const {
  return pinned_tab_manager_.GetPinnedTabs();
}

}  // namespace glic
