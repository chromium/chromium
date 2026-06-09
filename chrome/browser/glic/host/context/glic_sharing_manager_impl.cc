// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"

#include "chrome/browser/glic/common/future_browser_features.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/context/glic_focused_browser_manager_impl.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/context/glic_pinned_tab_manager_impl.h"
#include "chrome/browser/glic/host/context/glic_sharing_utils.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/url_constants.h"
#include "glic_pinned_tab_manager.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/host/context/glic_focused_browser_manager.h"
#include "chrome/browser/glic/host/context/glic_focused_tab_manager.h"
#endif

namespace glic {

// Returns whether tab context sharing is allowed.
// If kGlicDefaultTabContextSetting is enabled, this always returns true as
// permission is managed per-instance/tab rather than via a global preference.
bool IsGlicTabContextEnabled(PrefService* pref_service) {
  if (base::FeatureList::IsEnabled(features::kGlicDefaultTabContextSetting)) {
    return true;
  }
  return pref_service->GetBoolean(glic::prefs::kGlicTabContextEnabled);
}

namespace {
GlicGetContextResult TransformFetcherResult(
    base::expected<glic::mojom::GetContextResultPtr,
                   page_content_annotations::FetchPageContextErrorDetails>
        result) {
  if (result.has_value()) {
    return base::ok(std::move(result.value()));
  }

  GlicGetContextFromTabError glic_error_code;
  switch (result.error().error_code) {
    case page_content_annotations::FetchPageContextError::kUnknown:
      glic_error_code = GlicGetContextFromTabError::kUnknown;
      break;
    case page_content_annotations::FetchPageContextError::kWebContentsChanged:
      glic_error_code = GlicGetContextFromTabError::kWebContentsChanged;
      break;
    case page_content_annotations::FetchPageContextError::
        kPageContextNotEligible:
      glic_error_code = GlicGetContextFromTabError::kPageContextNotEligible;
      break;
    case page_content_annotations::FetchPageContextError::kWebContentsWentAway:
      glic_error_code = GlicGetContextFromTabError::kTabNotFound;
      break;
  }
  return base::unexpected(
      GlicGetContextError{glic_error_code, result.error().message});
}
}  // namespace

GlicSharingManagerImpl::GlicSharingManagerImpl(
    std::unique_ptr<GlicFocusedTabManagerInterface> focused_tab_manager,
    std::unique_ptr<GlicFocusedBrowserManager> focused_browser_manager,
    GlicPinnedTabManager* pinned_tab_manager,
    Profile* profile,
    GlicMetrics* metrics)
    : focused_browser_manager_(std::move(focused_browser_manager)),
      focused_tab_manager_(std::move(focused_tab_manager)),
      pinned_tab_manager_(pinned_tab_manager),
      profile_(profile),
      metrics_(metrics) {}

GlicSharingManagerImpl::~GlicSharingManagerImpl() = default;

GlicPinnedTabManager* GlicSharingManagerImpl::pinned_tab_manager() const {
  return std::visit(
      absl::Overload{
          [](const std::unique_ptr<GlicPinnedTabManager>& pinned_tab_manager) {
            return pinned_tab_manager.get();
          },
          [](const raw_ptr<GlicPinnedTabManager>& pinned_tab_manager) {
            return pinned_tab_manager.get();
          }},
      pinned_tab_manager_);
}

base::CallbackListSubscription
GlicSharingManagerImpl::AddFocusedTabChangedCallback(
    FocusedTabChangedCallback callback) {
  return focused_tab_manager_->AddFocusedTabChangedCallback(
      std::move(callback));
}

FocusedTabData GlicSharingManagerImpl::GetFocusedTabData() {
  return focused_tab_manager_->GetFocusedTabData();
}

base::CallbackListSubscription
GlicSharingManagerImpl::AddTabPinningStatusChangedCallback(
    TabPinningStatusChangedCallback callback) {
  return pinned_tab_manager()->AddTabPinningStatusChangedCallback(
      std::move(callback));
}

base::CallbackListSubscription
GlicSharingManagerImpl::AddTabPinningStatusEventCallback(
    TabPinningStatusEventCallback callback) {
  return pinned_tab_manager()->AddTabPinningStatusEventCallback(
      std::move(callback));
}

bool GlicSharingManagerImpl::PinTabs(
    base::span<const tabs::TabHandle> tab_handles,
    GlicPinTrigger trigger) {
  return pinned_tab_manager()->PinTabs(tab_handles, trigger);
}

void GlicSharingManagerImpl::SetPinTrigger(tabs::TabHandle tab_handle,
                                           GlicPinTrigger trigger) {
  pinned_tab_manager()->SetPinTrigger(tab_handle, trigger);
}

bool GlicSharingManagerImpl::UnpinTabs(
    base::span<const tabs::TabHandle> tab_handles,
    GlicUnpinTrigger trigger) {
  return pinned_tab_manager()->UnpinTabs(tab_handles, trigger);
}

void GlicSharingManagerImpl::UnpinAllTabs(GlicUnpinTrigger trigger) {
  pinned_tab_manager()->UnpinAllTabs(trigger);
}

std::optional<GlicPinnedTabUsage> GlicSharingManagerImpl::GetPinnedTabUsage(
    tabs::TabHandle tab_handle) {
  return pinned_tab_manager()->GetPinnedTabUsage(tab_handle);
}

int32_t GlicSharingManagerImpl::GetMaxPinnedTabs() const {
  return pinned_tab_manager()->GetMaxPinnedTabs();
}

int32_t GlicSharingManagerImpl::GetNumPinnedTabs() const {
  return pinned_tab_manager()->GetNumPinnedTabs();
}

bool GlicSharingManagerImpl::IsTabPinned(tabs::TabHandle tab_handle) const {
  return pinned_tab_manager()->IsTabPinned(tab_handle);
}

bool GlicSharingManagerImpl::IsTabFocused(tabs::TabHandle tab_handle) const {
  return focused_tab_manager_->IsTabFocused(tab_handle);
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
      : last_focused_browser_(
            GetBrowserWindowInterfaceWeakPtr(focused_browser)),
        callback_(std::move(callback)) {}

  void OnFocusedBrowserChanged(BrowserWindowInterface* candidate_browser,
                               BrowserWindowInterface* focused_browser) {
    if (last_focused_browser_.get() != focused_browser ||
        last_focused_browser_.WasInvalidated()) {
      callback_.Run(focused_browser);
    }
    last_focused_browser_ = GetBrowserWindowInterfaceWeakPtr(focused_browser);
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
  return focused_browser_manager_->AddFocusedBrowserChangedCallback(
      base::BindRepeating(
          &FocusedBrowserChangedWatcher::OnFocusedBrowserChanged,
          std::make_unique<FocusedBrowserChangedWatcher>(
              focused_browser_manager_->GetFocusedBrowser(),
              std::move(callback))));
}

BrowserWindowInterface* GlicSharingManagerImpl::GetFocusedBrowser() const {
  return focused_browser_manager_->GetFocusedBrowser();
}

base::CallbackListSubscription
GlicSharingManagerImpl::AddFocusedTabDataChangedCallback(
    FocusedTabDataChangedCallback callback) {
  return focused_tab_manager_->AddFocusedTabDataChangedCallback(
      std::move(callback));
}

base::CallbackListSubscription
GlicSharingManagerImpl::AddPinnedTabsChangedCallback(
    PinnedTabsChangedCallback callback) {
  return pinned_tab_manager()->AddPinnedTabsChangedCallback(
      std::move(callback));
}

base::CallbackListSubscription
GlicSharingManagerImpl::AddPinnedTabDataChangedCallback(
    PinnedTabDataChangedCallback callback) {
  return pinned_tab_manager()->AddPinnedTabDataChangedCallback(
      std::move(callback));
}

int32_t GlicSharingManagerImpl::SetMaxPinnedTabs(uint32_t max_pinned_tabs) {
  return pinned_tab_manager()->SetMaxPinnedTabs(max_pinned_tabs);
}

std::optional<GlicGetContextError>
GlicSharingManagerImpl::CheckPreliminaryContextSharingEligibility(
    tabs::TabHandle tab_handle) const {
  tabs::TabInterface* tab = tab_handle.Get();
  if (!tab) {
    return GlicGetContextError{GlicGetContextFromTabError::kTabNotFound,
                               "tab not found"};
  }

  const bool is_pinned = IsTabPinned(tab_handle);
  if (!is_pinned && !IsGlicTabContextEnabled(profile_->GetPrefs())) {
    return GlicGetContextError{
        GlicGetContextFromTabError::
            kPermissionDeniedContextPermissionNotEnabled,
        "permission denied: context permission not enabled"};
  }

  const bool is_focused = focused_tab_manager_->IsTabFocused(tab_handle);
  const bool is_shared = is_focused || is_pinned;
  if (!is_shared || !IsTabValidForSharing(tab->GetContents())) {
    return GlicGetContextError{GlicGetContextFromTabError::kPermissionDenied,
                               "permission denied"};
  }

  return std::nullopt;
}

void GlicSharingManagerImpl::GetContextFromTab(
    tabs::TabHandle tab_handle,
    const mojom::GetTabContextOptions& options,
    base::OnceCallback<void(GlicGetContextResult)> callback) {
  if (auto error = CheckPreliminaryContextSharingEligibility(tab_handle)) {
    std::move(callback).Run(base::unexpected(*error));
    return;
  }

  tabs::TabInterface* tab = tab_handle.Get();
  CHECK(tab);
  GetContextFromTabImpl(tab, options, std::move(callback));
}

void GlicSharingManagerImpl::GetContextForActorFromTab(
    tabs::TabHandle tab_handle,
    const mojom::GetTabContextOptions& options,
    base::OnceCallback<void(GlicGetContextResult)> callback) {
  auto* tab = tab_handle.Get();
  if (!tab) {
    std::move(callback).Run(base::unexpected(GlicGetContextError{
        GlicGetContextFromTabError::kTabNotFound, "tab not found"}));
    return;
  }
  if (tab->GetProfile() != profile_) {
    std::move(callback).Run(base::unexpected(GlicGetContextError{
        GlicGetContextFromTabError::kPermissionDenied, "profile mismatch"}));
    return;
  }
  if (!IsTabValidForSharing(tab)) {
    std::move(callback).Run(base::unexpected(GlicGetContextError{
        GlicGetContextFromTabError::kPermissionDenied, "permission denied"}));
    return;
  }
  GetContextFromTabImpl(tab, options, std::move(callback));
}

std::vector<tabs::TabInterface*> GlicSharingManagerImpl::GetPinnedTabs() const {
  return pinned_tab_manager()->GetPinnedTabs();
}

void GlicSharingManagerImpl::SubscribeToPinCandidates(
    mojom::GetPinCandidatesOptionsPtr options,
    mojo::PendingRemote<mojom::PinCandidatesObserver> observer) {
  pinned_tab_manager()->SubscribeToPinCandidates(std::move(options),
                                                 std::move(observer));
}

void GlicSharingManagerImpl::OnConversationTurnSubmitted() {
  pinned_tab_manager()->OnAllPinnedTabsContextEvent(GlicPinnedTabContextEvent(
      GlicPinnedTabContextEventType::kConversationTurnSubmitted));
}

base::WeakPtr<GlicSharingManagerInternal> GlicSharingManagerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void GlicSharingManagerImpl::GetContextFromTabImpl(
    tabs::TabInterface* tab,
    const mojom::GetTabContextOptions& options,
    base::OnceCallback<void(GlicGetContextResult)> callback) {
  FetchPageContext(
      tab, options,
      base::BindOnce(&TransformFetcherResult).Then(std::move(callback)),
      /*progress_listener=*/nullptr,
      /*is_screenshot_annotated=*/false);
}

}  // namespace glic
